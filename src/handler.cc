/*
Copyright (c) 2015-2019, Robert J. Hansen <rjh@sixdemonbag.org>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <algorithm>
#include <boost/tokenizer.hpp>
#include <exception>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>
#include "main.h"

using boost::char_separator;
using boost::tokenizer;
using boost::asio::ip::tcp;
using std::back_inserter;
using std::binary_search;
using std::exception;
using std::getline;
using std::pair;
using std::string;
using std::stringstream;
using std::to_string;
using std::transform;
using std::vector;
using std::distance;

// defined in main.cc
extern const vector<pair64>& hashes;

namespace {
enum class Command {
  Version = 0,
  Bye = 1,
  Status = 2,
  Query = 3,
  Upshift = 4,
  Downshift = 5,
  Unknown = 6
};

auto tokenize(const string&& line) {
  vector<string> rv;
  char_separator<char> sep(" ");
  tokenizer<char_separator<char>> tokens(line, sep);
  for (const auto& t : tokens) {
    rv.emplace_back(t);
  }
  return rv;
}

bool is_present_in_hashes(const string& hash) {
  return binary_search(hashes.cbegin(), hashes.cend(), to_pair64(hash));
}

auto getCommand(const string& cmdstring) {
  string localcmd = "";
  transform(cmdstring.cbegin(), cmdstring.cend(), back_inserter(localcmd),
            ::toupper);

  auto cmd = Command::Unknown;

  if (localcmd == "VERSION:")
    cmd = Command::Version;
  else if (localcmd == "BYE")
    cmd = Command::Bye;
  else if (localcmd == "STATUS")
    cmd = Command::Status;
  else if (localcmd == "QUERY")
    cmd = Command::Query;
  else if (localcmd == "UPSHIFT")
    cmd = Command::Upshift;
  else if (localcmd == "DOWNSHIFT")
    cmd = Command::Downshift;

  return cmd;
}
}  // namespace

void handle_client(tcp::iostream& stream) {
  const string ipaddr = stream.socket().remote_endpoint().address().to_string();
  unsigned long long queries = 0;
  try {
    bool byebye = false;
    while (stream && (! byebye)) {
      string line;
      getline(stream, line);
      // trim leading/following whitespace
      auto end_ws = line.find_last_not_of("\t\n\v\f\r ");

      // trips on the empty string, or a string of pure whitespace
      if (line.size() == 0 || end_ws == string::npos)
	break;
      
      auto head_iter = line.cbegin() + line.find_first_not_of("\t\n\v\f\r ");
      auto end_iter = line.cbegin() + end_ws + 1;
      auto commands = tokenize(string(head_iter, end_iter));
      
      switch (getCommand(commands.at(0))) {
        case Command::Version:
          stream << "OK\r\n";
          break;

        case Command::Bye:
	  byebye = true;
          break;

        case Command::Status:
          stream << "NOT SUPPORTED\r\n";
          break;

        case Command::Query: {
          stringstream rv;
          rv << "OK ";
          for (size_t idx = 1; idx < commands.size(); ++idx)
            rv << (is_present_in_hashes(commands.at(idx)) ? "1" : "0");
          rv << "\r\n";
          queries += (commands.size() - 1);
          stream << rv.str();
          break;
        }

        case Command::Upshift:
          stream << "NOT OK\r\n";
          break;

        case Command::Downshift:
          stream << "NOT OK\r\n";
          break;

        case Command::Unknown:
          stream << "NOT OK\r\n";
	  byebye = true;
      }
    }
  } catch (std::exception& e) {
    log(LogLevel::ALERT, string("Error: ") + e.what());
    // swallow the exception: we'll close the connection
    // automagically on exit
    //
    // fall-through here to function return
  }

  stringstream status_msg;
  status_msg << ipaddr << " closed session after " << queries << " queries";
  log(LogLevel::ALERT, status_msg.str());
}
