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
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include "main.h"

using std::hex;
using std::invalid_argument;
using std::make_pair;
using std::pair;
using std::regex;
using std::regex_match;
using std::setfill;
using std::setw;
using std::string;
using std::stringstream;

string from_pair64(const pair64& input) {
  stringstream stream;
  stream << setfill('0') << setw(sizeof(unsigned long long) * 2) << hex
         << input.first << input.second;
  return string(stream.str());
}

pair64 to_pair64(const string& input) {
  static const regex md5_re{"^[A-Fa-f0-9]{32}$"};

  if (!regex_match(input.cbegin(), input.cend(), md5_re))
    throw invalid_argument("not a hash");

  auto first = string(input.cbegin(), input.cbegin() + 16);
  auto second = string(input.cbegin() + 16, input.cend());
  auto left = std::strtoull(first.c_str(), nullptr, 16);
  auto right = std::strtoull(second.c_str(), nullptr, 16);

  return make_pair(left, right);
}

bool operator<(const pair64& lhs, const pair64& rhs) {
  return (lhs.first < rhs.first) or
         (lhs.first == rhs.first and lhs.second < rhs.second);
}

bool operator==(const pair64& lhs, const pair64& rhs) {
  return (lhs.first == rhs.first) and (lhs.second == rhs.second);
}

bool operator>(const pair64& lhs, const pair64& rhs) {
  return ((!(lhs < rhs)) and (!(lhs == rhs)));
}
