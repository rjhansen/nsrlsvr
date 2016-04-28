/*
Copyright (c) 2015-2016, Robert J. Hansen <rjh@sixdemonbag.org>

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

#include "main.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <regex>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#ifdef __FreeBSD__
#include <netinet/in.h>
#include <sys/socket.h>
#endif

using std::string;
using std::transform;
using std::ifstream;
using std::cerr;
using std::cout;
using std::vector;
using std::sort;
using std::pair;
using std::regex;
using std::stoi;
using std::to_string;
using std::getline;

namespace {
vector<pair64> hash_set;
string hashes_location{ PKGDATADIR "/hashes.txt" };
uint16_t port{ 9120 };

/** Attempts to load a set of MD5 hashes from disk.
  * Each line must be either blank or 32 hexadecimal digits.  If the
  * file doesn't conform to this, nsrlsvr will abort and display an
  * error message to the log.
  */
void
load_hashes()
{
  const regex md5_re{ "^[A-Fa-f0-9]{32}$" };
  vector<char> buf(1024, 0);
  uint32_t hash_count{ 0 };
  ifstream infile{ hashes_location.c_str() };

  // As of this writing, the RDS had about 40 million entries.
  // When a vector needs to grow, it normally does so by doubling
  // the former allocation -- so after this, the next stop is a
  // 90 million allocation (@ 16 bytes per, or 1.4 GiB).  If you're
  // maintaining this code, try to keep the reserve a few million
  // larger than the RDS currently is, to give yourself room to
  // grow without a vector realloc.
  //
  // Failure to reserve this block of memory is non-recoverable.
  // Don't even try.  Just log the error and bail out.  Let the end
  // user worry about installing more RAM.
  try {
    hash_set.reserve(45000000);
  } catch (std::bad_alloc&) {
    log(LogLevel::ALERT, "couldn't reserve enough memory");
    exit(EXIT_FAILURE);
  }

  if (not infile) {
    log(LogLevel::ALERT, "couldn't open hashes file " + hashes_location);
    exit(EXIT_FAILURE);
  }

  while (infile) {
    string line;
    getline(infile, line);
    transform(line.begin(), line.end(), line.begin(), ::toupper);
    if (0 == line.size())
      continue;

    if (!regex_match(line.cbegin(), line.cend(), md5_re)) {
      log(LogLevel::ALERT, "hash file appears corrupt!  Loading no hashes.");
      log(LogLevel::ALERT, "offending line is: " + line);
      log(LogLevel::ALERT, "shutting down!");
      exit(EXIT_FAILURE);
    }

    try {
      // .emplace_back is the C++11 improvement over the old
      // vector.push_back.  It has the benefit of not needing
      // to construct a temporary to hold the value; it can
      // just construct-in-place.  For 40 million values, that
      // can be significant.
      //
      // Note that if the vector runs out of reserved room it
      // will attempt to make a new allocation double the size
      // of the last.  That means the application will at least
      // briefly need *three times* the expected RAM -- one for
      // the data set and two for the newly-allocated chunk.
      // Given we're talking about multiple gigs of RAM, this
      // .emplace_back needs to consider the possibility of a
      // RAM allocation failure.
      hash_set.emplace_back(to_pair64(line));
      hash_count += 1;
      if (0 == hash_count % 1000000) {
        string howmany{ to_string(hash_count / 1000000) };
        log(LogLevel::ALERT, "loaded " + howmany + " million hashes");
      }
    } catch (std::bad_alloc&) {
      log(LogLevel::ALERT, "couldn't allocate enough memory");
      exit(EXIT_FAILURE);
    }
  }
  string howmany{ to_string(hash_count) };
  log(LogLevel::INFO, "read in " + howmany + " hashes");

  infile.close();

  sort(hash_set.begin(), hash_set.end());

  if (hash_set.size() > 1) {
    log(LogLevel::INFO, "ensuring no duplicates");
    pair64 foo{ hash_set.at(0) };
    for (auto iter = (hash_set.cbegin() + 1); iter != hash_set.cend(); ++iter) {
      if (foo == *iter) {
        log(LogLevel::ALERT, "hash file contains duplicates -- "
                             "shutting down!");
        exit(EXIT_FAILURE);
      }
      foo = *iter;
    }
  }
}

/** Converts this process into a well-behaved UNIX daemon.*/
void
daemonize()
{
  /* Nothing in here should be surprising.  If it is, then please
     check the standard literature to ensure you understand how a
     daemon is supposed to work. */
  const auto pid = fork();
  if (0 > pid) {
    log(LogLevel::WARN, "couldn't fork!");
    exit(EXIT_FAILURE);
  } else if (0 < pid) {
    exit(EXIT_SUCCESS);
  }
  log(LogLevel::INFO, "daemon started");

  umask(0);

  if (0 > setsid()) {
    log(LogLevel::WARN, "couldn't set sid");
    exit(EXIT_FAILURE);
  }

  if (0 > chdir("/")) {
    log(LogLevel::WARN, "couldn't chdir to root");
    exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}

/** Creates a server socket to listen for client connections. */
auto
make_socket()
{
  /* If anything in here is surprising, please check the standard
     literature to make sure you understand TCP/IP. */

  sockaddr_in server;
  memset(static_cast<void*>(&server), 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(port);

  const auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    log(LogLevel::WARN, "couldn't create a server socket");
    exit(EXIT_FAILURE);
  }
  if (0 > bind(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server))) {
    log(LogLevel::WARN, "couldn't bind to port");
    exit(EXIT_FAILURE);
  }
  if (0 > listen(sock, 20)) {
    log(LogLevel::WARN, "couldn't listen for clients");
    exit(EXIT_FAILURE);
  }
  log(LogLevel::INFO, "ready for clients");

  return sock;
}

/** Display a helpful usage message. */
void
show_usage(string program_name)
{
  cout << "Usage: " << program_name
       << " [-vbh -f FILE -p PORT]\n\n"
          "-v : print version information\n"
          "-b : get information on reporting bugs\n"
          "-f : specify an alternate hash set (default: \n     " PKGDATADIR
          "/hashes.txt)\n"
          "-h : show this help message\n"
          "-p : listen on PORT, between 1024 and 65535 (default: 9120)\n\n";
}

/** Parse command-line options.
    @param argc argc from main()
    @param argv argv from main()
*/
void
parse_options(int argc, char* argv[])
{
  int32_t opt{ 0 };

  while (-1 != (opt = getopt(argc, argv, "vbhf:p:"))) {
    switch (opt) {
      case 'v':
        cout << argv[0] << " " << PACKAGE_VERSION << "\n\n";
        exit(EXIT_SUCCESS);

      case 'b':
        cout
          << argv[0] << " " << PACKAGE_VERSION << "\n"
          << PACKAGE_URL << "\n"
                            "Praise, blame and bug reports to "
          << PACKAGE_BUGREPORT
          << ".\n\n"
             "Please be sure to include your operating system, version of "
             "your\n"
             "operating system, and a detailed description of how to recreate\n"
             "your bug.\n\n";
        exit(EXIT_SUCCESS);

      case 'f': {
        hashes_location = string(static_cast<const char*>(optarg));
        ifstream infile{ hashes_location.c_str() };
        if (not infile) {
          cerr << "Error: the specified dataset file could not be found.\n\n";
          exit(EXIT_FAILURE);
        }
        break;
      }
      case 'h':
        show_usage(argv[0]);
        exit(EXIT_SUCCESS);

      case 'p':
        try {
          auto port_num = static_cast<uint16_t>(stoi(optarg));
          if (port_num < 1024 || port_num > 65535)
            throw new std::exception();
          port = port_num;
        } catch (...) {
          cerr << "Error: invalid value for port\n\n";
          exit(EXIT_FAILURE);
        }
        break;
      default:
        show_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }
}
}

/** The set of all loaded hashes, represented as a const reference. */
const vector<pair64>& hashes{ hash_set };

/** Writes to syslog with the given priority level.

    @param level The priority of the message
    @param msg The message to write
*/
void
log(const LogLevel level, const string&& msg)
{
  syslog(LOG_MAKEPRI(LOG_USER, static_cast<int>(level)), "%s", msg.c_str());
}

/** Entry point for the application.

    @param argc The number of command-line arguments
    @param argv Command-line arguments
*/
int
main(int argc, char* argv[])
{
  parse_options(argc, argv);
  daemonize();
  load_hashes();
  // The following line helps avoid zombie processes.  Normally parents
  // need to reap their children in order to prevent zombie processes;
  // if SIGCHLD is set to SIG_IGN, though, the processes can terminate
  // normally.
  signal(SIGCHLD, SIG_IGN);
  int32_t client_sock{ 0 };
  int32_t svr_sock{ make_socket() };
  sockaddr_in client;
  sockaddr* client_addr = reinterpret_cast<sockaddr*>(&client);
  socklen_t client_length{ sizeof(client) };

  while (true) {
    if (0 > (client_sock = accept(svr_sock, client_addr, &client_length))) {
      log(LogLevel::WARN, "could not accept connection");
      switch (errno) {
        case EAGAIN:
          log(LogLevel::WARN, "-- EAGAIN");
          break;
        case ECONNABORTED:
          log(LogLevel::WARN, "-- ECONNABORTED");
          break;
        case EINTR:
          log(LogLevel::WARN, "-- EINTR");
          break;
        case EINVAL:
          log(LogLevel::WARN, "-- EINVAL");
          break;
        case EMFILE:
          log(LogLevel::WARN, "-- EMFILE");
          break;
        case ENFILE:
          log(LogLevel::WARN, "-- ENFILE");
          break;
        case ENOTSOCK:
          log(LogLevel::WARN, "-- ENOTSOCK");
          break;
        case EOPNOTSUPP:
          log(LogLevel::WARN, "-- EOPNOTSUPP");
          break;
        case ENOBUFS:
          log(LogLevel::WARN, "-- ENOBUFS");
          break;
        case ENOMEM:
          log(LogLevel::WARN, "-- ENOMEM");
          break;
        case EPROTO:
          log(LogLevel::WARN, "-- EPROTO");
          break;
        default:
          log(LogLevel::WARN, "-- EUNKNOWN");
          break;
      }
      continue;
    }

    string ipaddr{ inet_ntoa(client.sin_addr) };
    log(LogLevel::ALERT, string("accepted a client: ") + ipaddr);

    if (0 == fork()) {
      log(LogLevel::ALERT, "calling handle_client");
      handle_client(client_sock);
      if (-1 == close(client_sock)) {
        log(LogLevel::WARN, string("Could not close client: ") + ipaddr);
        switch (errno) {
          case EBADF:
            log(LogLevel::WARN, "-- EBADF");
            break;
          case EINTR:
            log(LogLevel::WARN, "-- EINTR");
            break;
          case EIO:
            log(LogLevel::WARN, "-- EIO");
            break;
        }
      } else {
        log(LogLevel::ALERT, string("closed client ") + ipaddr);
      }
      return 0;
    } else {
      if (-1 == close(client_sock)) {
        log(LogLevel::WARN, string("Parent could not close client: ") + ipaddr);
        switch (errno) {
          case EBADF:
            log(LogLevel::WARN, "-- EBADF");
            break;
          case EINTR:
            log(LogLevel::WARN, "-- EINTR");
            break;
          case EIO:
            log(LogLevel::WARN, "-- EIO");
            break;
        }
      }
    }
  }

  // Note that as is normal for daemons, the exit point is never
  // reached.  This application does not normally terminate.
  return EXIT_SUCCESS;
}
