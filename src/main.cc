/* Copyright (c) 2011-2014, Robert J. Hansen <rjh@secret-alchemy.com>
 * and others.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Code standards:
 *   This is a small enough project we don't need a formal coding standard.
 *   That said, here are some helpful tips for people who want to submit
 *   patches:
 *
 *   - If it's not 100% ISO C++11, it won't get in.
 *   - It must compile cleanly and without warnings under both GNU G++
 *     and Clang++, even with "-W -Wextra -ansi -pedantic".
 *     (Exceptions can be made for warnings that are actually compiler
 *     conformance issues: for instance, Clang++ will warn that 'long long'
 *     is a C++11 extension, even when you run it in -std=C++11 mode.)
 *   - C++ offers 'and', 'or' and 'not' keywords instead of &&, || and !.
 *     I like these: I think they're more readable.  Please use them.
 *   - C++11 uniform initialization syntax, please.
 *   - Please try to follow the formatting conventions.  It's mostly
 *     straight-up astyle format, with occasional tweaks where necessary
 *     to get nice hardcopy printouts.
 *   - If you write a new function it must have a Doxygen block
 *     documenting it.
 *
 * Contributor history:
 *
 * Robert J. Hansen <rjh@secret-alchemy.com>
 *   - most everything
 */

#include "../config.h"
#include <sstream>
#include <sys/stat.h>
#include <syslog.h>
#include <set>
#include <string>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <algorithm>
#include <limits.h>
#include "handler.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <utility>

/* Additional defines necessary on FreeBSD: */
/* Necessary for sockaddr and sockaddr_in structures */
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <netinet/in.h>
#endif

using std::stringstream;
using std::string;
using std::set;
using std::transform;
using std::find_if;
using std::not1;
using std::ptr_fun;
using std::ifstream;
using std::cerr;
using std::vector;
using std::remove_if;
using std::sort;
using std::pair;
using std::unique_ptr;

typedef unsigned long long ULONG64;
typedef pair<ULONG64, ULONG64> pair64;

#define INFO LOG_MAKEPRI(LOG_USER, LOG_INFO)
#define WARN LOG_MAKEPRI(LOG_USER, LOG_WARNING)
#define DEBUG LOG_MAKEPRI(LOG_USER, LOG_DEBUG)
#define CRITICAL LOG_MAKEPRI(LOG_USER, LOG_CRIT)
#define ALERT LOG_MAKEPRI(LOG_USER, LOG_ALERT)
#define EMERGENCY LOG_MAKEPRI(LOG_USER, LOG_EMERG)
#define MAX_PENDING_REQUESTS 20
#define BUFFER_SIZE 128

namespace
{
/** Tracks whether the server should only support protocol 1.0. */
bool old_only { false };

/** Tracks whether the server should support status queries. */
bool status_enabled { false };

/** Tracks whether the server should run as a daemon. */
bool standalone { false };

/** Our set of hashes, represented as a block of contiguous memory.
  * Note that the current NSRL library contains approximately 40
  * million values, each at roughly 32 bytes (rounded to binary
  * powers to make the math easier).  This is 2**25 values times
  * 2**5 bytes each = 2**30 bytes, or about a gig of RAM.
  *
  * Moral of the story: populating this set is computationally
  * expensive. */
vector<pair64> hash_set;

/** Tracks where we look for the location of the
  * reference data set. */
string RDS_LOC { PKGDATADIR "/hashes.txt" };

/** Which port to listen on */
uint16_t PORT { 9120 };

/** Determines whether a string represents a valid uppercase
  * MD5 hash. */

auto good_line(const string& line)
{
    if (line.size() != 32)
        return false;

    auto iter = line.cbegin();
    auto end = line.cend();
    while (iter != end and
            ((*iter >= '0' and *iter <= '9') or
             (*iter >= 'A' and *iter <= 'F')))
        ++iter;
    return (iter == end) ? true : false;
}

/** Loads hashes from disk and stores them in a fast-accessing
  * in-memory data structure.  This will be slow. */
void load_hashes()
{
    vector<char> buf(BUFFER_SIZE, 0);
    uint32_t line_count { 0 }, hash_count { 0 };
    ifstream infile { RDS_LOC.c_str() };
	
	hash_set.reserve(40000000);

    if (not infile)
    {
        syslog(WARN, "couldn't open hashes file %s",
               RDS_LOC.c_str());
        exit(EXIT_FAILURE);
    }

    while (infile)
    {
        memset(static_cast<void*>(&buf[0]), 0, BUFFER_SIZE);
        infile.getline(&buf[0], BUFFER_SIZE);
        line_count += 1;
        const string line(buf.cbegin(), find(buf.cbegin(), buf.cend(), 0));
		
        if (0 == line.size())
        {
            continue;
        }

        if (! good_line(line))
        {
            syslog(ALERT, "hash file appears corrupt!  Loading no hashes.");
            syslog(ALERT, "offending line is #%d", line_count);
            syslog(ALERT, "offending line has %d bytes", (int) line.size());
            syslog(ALERT, "Content follows:");
            syslog(ALERT, "%s", line.c_str());
            syslog(ALERT, "shutting down!");
            exit(EXIT_FAILURE);
            return;
        }
        hash_count += 1;

        if (0 == hash_count % 1000000)
        {
            syslog(INFO, "loaded %u million hashes", hash_count / 1000000);
        }
		
        hash_set.push_back(to_pair64(line));
    }
    infile.close();
    syslog(INFO, "read in %u unique hashes",
           static_cast<uint32_t>(hash_set.size()));
    sort(hash_set.begin(), hash_set.end());
}

/** Converts our application into a proper daemon. */
void daemonize()
{
    const auto pid = fork();
    if (pid < 0)
    {
        syslog(WARN, "couldn't fork!");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    syslog(INFO, "daemon started");
    umask(0);

    if (setsid() < 0)
    {
        syslog(WARN, "couldn't set sid");
        exit(EXIT_FAILURE);
    }
    // Technically, the root directory is the only one guaranteed
    // to exist on the filesystem.  Therefore, it's the only safe
    // directory to point our daemon at.  I doubt this is strictly
    // necessary, but remembering to completely rebase a daemon is
    // part of just good hacking etiquette.
    if (0 > chdir("/"))
    {
        syslog(WARN, "couldn't chdir to root");
        exit(EXIT_FAILURE);
    }
    // No extraneous filehandles for us.  Daemons lack stdio, so
    // shut 'em on down.
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}


/** Creates a server socket that will listen for clients. */
auto make_socket()
{
    const auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) ;
    if (sock < 0)
    {
        syslog(WARN, "couldn't create a server socket");
        exit(EXIT_FAILURE);
    }
	
    sockaddr_in server;
    memset(static_cast<void*>(&server), 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    if (0 > bind(sock, reinterpret_cast<sockaddr*>(&server),
                 sizeof(server)))
    {
        syslog(WARN, "couldn't bind to port 9120");
        exit(EXIT_FAILURE);
    }
    if (0 > listen(sock, MAX_PENDING_REQUESTS))
    {
        syslog(WARN, "couldn't listen for clients");
        exit(EXIT_FAILURE);
    }
    syslog(INFO, "ready for clients");
    return sock;
}

/** Checks a string to see if it's a valid base-10 number. */
auto is_num(const string& num)
{
    auto b = num.cbegin();
    auto e = num.cend();

    return (e == find_if(b, e, not1(ptr_fun(::isdigit))))
           ? ::atoi(num.c_str())
           : -1;
}

void show_usage(const char* program_name)
{
    cerr <<
         "Usage: " << program_name << " [-vbhsSo -f FILE -p PORT -t TIMEOUT]\n\n" <<
         "-v : print version information\n" <<
         "-b : get information on reporting bugs\n" <<
         "-f : specify an alternate RDS (default: "<< PKGDATADIR <<
         "/NSRLFile.txt)\n" <<
         "-s : allow clients to query server status (default: disabled)\n" <<
         "-h : show this help message\n" <<
         "-p : listen on PORT, between 1024 and 65535 (default: 9120)\n\n";
    exit(EXIT_FAILURE);
}

void parse_options(int argc, char* argv[])
{
    int32_t opt { 0 };
    
    while (-1 != (opt = getopt(argc, argv, "bsvof:hp:t:S")))
    {
        switch (opt)
        {
        case 'v':
            cerr << argv[0] << " " << PACKAGE_VERSION << "\n\n";
            exit(0);
            break;
        case 'b':
            cerr << argv[0] << " " << PACKAGE_VERSION
                 << "\n" << PACKAGE_URL << "\n" <<
                 "Praise, blame and bug reports to " << PACKAGE_BUGREPORT << ".\n\n" <<
                 "Please be sure to include your operating system, version of your\n" <<
                 "operating system, and a detailed description of how to recreate\n" <<
                 "your bug.\n\n";
            exit(0);
            break;
        case 'f':
			{
            	RDS_LOC = string((const char*) optarg);
            	auto infile = ifstream(RDS_LOC.c_str());
            	if (not infile)
            	{
                	cerr <<
                    	 "Error: the specified dataset file could not be found.\n\n";
                	exit(EXIT_FAILURE);
            	}
			}
            break;
        case 'h':
            show_usage(argv[0]);
            exit(0);
            break;
        case 'p':
			{
            	auto port_num = string(optarg);
				stringstream ss;
				ss << port_num;
				ss >> PORT;
				if (! ss)
				{
					show_usage(argv[0]);
					exit(EXIT_FAILURE);
				}
			}
            break;
        case 's':
            status_enabled = true;
            break;
        default:
            show_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }    
}

}
/** An externally available const reference to the hash set. */
const vector<pair64>& hashes { hash_set };

/** An externally available const reference to the variable storing
  * whether or not status checking should be enabled. */
const bool& enable_status { status_enabled };


/** magic happens here */
int main(int argc, char* argv[])
{
    parse_options(argc, argv);
    daemonize();
    load_hashes();
    
	int32_t client_sock {0};
    int32_t svr_sock { make_socket() };
	sockaddr_in client;
	socklen_t client_length { sizeof(client) };
    bool in_parent = true;
    
    signal(SIGCHLD, SIG_IGN);
    
SERVER_LOOP:
    if (0 > (client_sock = accept(svr_sock,
                                  reinterpret_cast<sockaddr*>(&client),
                                  &client_length)))
    {
        syslog(WARN, "dropped a connection");
    }
    else
    {
		string ipaddr { inet_ntoa(client.sin_addr) };
		if (0 == fork()) {
			in_parent = false;
			handle_client(client_sock, ipaddr);
        }
    }
    if (in_parent)
        goto SERVER_LOOP;
}
