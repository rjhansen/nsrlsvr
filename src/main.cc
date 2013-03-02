/* $Id: main.cc 142 2013-02-23 22:25:32Z rjh $
 *
 * Copyright (c) 2011-2012, Robert J. Hansen <rjh@secret-alchemy.com>
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
 *   - If it's not 100% ISO C++98, it won't get in.
 *   - It must compile cleanly and without warnings under both GNU G++
 *     and Clang++, even with "-W -Wextra -ansi -pedantic".
 *   - C++ offers 'and', 'or' and 'not' keywords instead of &&, || and !.  
 *     I like these: I think they're more readable.  Please use them.
 *   - C++ allows you to initialize variables at declaration time by
 *     doing something like "int x(3)" instead of "int x = 3".  Please
 *     do this where practical: it's a good habit to get into for C++.
 *   - Please try to follow the formatting conventions.  It's mostly
 *     straight-up astyle format, with occasional tweaks where necessary
 *     to get nice hardcopy printouts.
 *   - If you write a new function it must have a Doxygen block
 *     documenting it.
 *
 * Contributor history:
 * Robert J. Hansen <rjh@secret-alchemy.com>
 *   - everything
 */

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

/* Additional defines necessary on FreeBSD: */
/* Necessary for sockaddr and sockaddr_in structures */
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <netinet/in.h>
#endif

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

#define INFO LOG_MAKEPRI(LOG_USER, LOG_INFO)
#define WARN LOG_MAKEPRI(LOG_USER, LOG_WARNING)
#define DEBUG LOG_MAKEPRI(LOG_USER, LOG_DEBUG)
#define CRITICAL LOG_MAKEPRI(LOG_USER, LOG_CRIT)
#define ALERT LOG_MAKEPRI(LOG_USER, LOG_ALERT)
#define EMERGENCY LOG_MAKEPRI(LOG_USER, LOG_EMERG)
#define MAX_PENDING_REQUESTS 20
#define BUFFER_SIZE 128

namespace {

/** Tracks whether the server should only support protocol 1.0. */
bool old_only(false);

/** Tracks whether the server should support status queries. */
bool status_enabled(false);

/** Tracks whether the server should run as a daemon. */
bool standalone(false);

/** Our set of hashes, represented as a set of strings.  Note
  * that the current NSRL library contains approximately 32
  * million values, each at roughly 64 bytes (rounded to binary
  * powers to make the math easier).  This is 2**25 values times
  * 2**6 bytes each = 2**31 bytes, or about two gigs of RAM.
  *
  * Moral of the story: populating this set is computationally
  * expensive. */
set<string> hash_set;

/** Tracks where we look for the location of the
  * reference data set. */
string RDS_LOC(PKGDATADIR "/NSRLFile.txt");

/** Keeps track of the last time we serviced a request.
  * This is locked via the active_sessions_mutex mutex.*/
time_t last_req_at(time(0));

/** Keeps track of how many clients are currently being serviced.
  * This is locked via the active_sessions_mutex mutex. */
int32_t active_sessions(0);

/** A mutex to keep various threads from clobbering each other
  * in their fanatical zeal to update shared resources.
  *
  * Interestingly, PTHREAD_MUTEX_INITIALIZER is so complex that
  * it cannot be used in a C++ initializer: you have to use old
  * C-style equals-operator initialization. */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/** The server's inactivity timeout interval */
int32_t TIMEOUT(INT_MAX);

/** Which port to listen on */
uint16_t PORT(9120);

/** A convenience class allowing us to pass multiple pieces of
    data with a void*. */
struct clientinfo {
    clientinfo(int32_t sfd, const char* ipaddr) : 
        sock_fd(sfd), ip_address(ipaddr) {}
    int32_t sock_fd;
    string ip_address;
};


/** Determines whether a string represents a valid uppercase
  * hexadecimal digit. */

class is_hexit : public std::unary_function<const string&, bool>
{
	public:
	is_hexit(size_t proper_len) : plen(proper_len) {}
	bool operator()(const string& line) const
	{
		if (line.size() != plen)
			return false;

		string::const_iterator iter(line.begin());
		const string::const_iterator end(line.end());
		while (iter != end and 
			((*iter >= '0' and *iter <= '9') or
			 (*iter >= 'A' and *iter <= 'F')))
			++iter;
		return (iter == end) ? true : false;
	}
	private:
	const size_t plen;
};

/** Loads hashes from disk and stores them in a fast-accessing
  * in-memory data structure.  This will be slow. */
void load_hashes()
{
    vector<char> buf(BUFFER_SIZE, 0);
    uint32_t line_count(0), hash_count(0);
    ifstream infile(RDS_LOC.c_str());

    if (not infile.good()) {
        syslog(WARN, "couldn't open hashes file %s",
               RDS_LOC.c_str());
        exit(EXIT_FAILURE);
    }

    infile.getline(&buf[0], BUFFER_SIZE);
    const string firstline(buf.begin(), find(buf.begin(), buf.end(), 0));
    const size_t firstline_size = firstline.size();
    line_count += 1;
    hash_count += 1;

    if (32 != firstline_size && 40 != firstline_size && 64 != firstline_size && 128 != firstline_size) {
        syslog(ALERT, "hash file appears corrupt!");
        syslog(ALERT, "error is in the first line.  Content follows:");
        syslog(ALERT, "%s", firstline.c_str());
        syslog(ALERT, "length of line = %d", (int)(firstline_size));
        syslog(ALERT, "shutting down!");
        infile.close();
        exit(EXIT_FAILURE);
        return;
    }
    is_hexit line_test(firstline_size);
    if (! line_test(firstline)) {
        syslog(ALERT, "hash file appears corrupt!  Loading no hashes.");
        syslog(ALERT, "error is in the first line.  Content follows:");
        syslog(ALERT, "%s", firstline.c_str());
        syslog(ALERT, "content fails is-all-hexadecimal check");
        syslog(ALERT, "shutting down!");
        infile.close();
        exit(EXIT_FAILURE);
        return;
    }
    hash_set.insert(firstline);

    while (infile) {
        // Per the C++ spec, &vector<T>[loc] is guaranteed
        // to be a T*.  (Unless it's a vector<bool>, in which
        // case you're living in such sin there's absolutely
        // no help for you.  Friends don't let friends use
        // vector<bool>.)
        memset(static_cast<void*>(&buf[0]), 0, BUFFER_SIZE);
        infile.getline(&buf[0], BUFFER_SIZE);
        line_count += 1;
        const string line(buf.begin(), find(buf.begin(), buf.end(), 0));
        if (0 == line.size()) {
            continue;
        }

        if (! line_test(line)) {
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

        if (0 == hash_count % 1000000) {
            syslog(INFO, "loaded %u million hashes", hash_count / 1000000);
        } 

        hash_set.insert(line);
    }
    infile.close();
    syslog(INFO, "read in %u unique hashes",
           static_cast<uint32_t>(hash_set.size()));
}

/** A thin wrapper around handler.cc and handle_client, meant
  * to ensure the programmer of that function doesn't have to
  * worry about thread contention. */
void* run_client_thread(void* arg)
{
    clientinfo* ci(static_cast<clientinfo*>(arg));
    const int32_t sock_fd(ci->sock_fd);
    const string ip_address(ci->ip_address);
    
    // Delete the dynamically-allocated memory block.  This
    // is an inevitable line of execution after successfully
    // allocating the block in the main loop (below).
    delete ci;

    if (0 != pthread_mutex_lock(&mutex)) {
        syslog(WARN, "couldn't acquire the mutex!");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    last_req_at = time(0);
    active_sessions += 1;
    if (0 != pthread_mutex_unlock(&mutex)) {
        syslog(WARN, "couldn't release the mutex!");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    syslog(INFO, "connection from %s", ip_address.c_str());

    handle_client(sock_fd, ip_address);

    close(sock_fd);

    syslog(INFO, "disconnected from %s", ip_address.c_str());

    if (0 != pthread_mutex_lock(&mutex)) {
        syslog(WARN, "couldn't acquire the mutex!");
        exit(-1);
    }
    active_sessions -= 1;
    if (0 != pthread_mutex_unlock(&mutex)) {
        syslog(WARN, "couldn't release the mutex!");
        exit(EXIT_FAILURE);
    }
    return NULL;
}


/** Converts our application into a proper daemon. */
void daemonize()
{
    const pid_t pid(fork());
    if (pid < 0) {
        syslog(WARN, "couldn't fork!");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    syslog(INFO, "daemon started");
    umask(0);

    if (setsid() < 0) {
        syslog(WARN, "couldn't set sid");
        exit(EXIT_FAILURE);
    }
    // Technically, the root directory is the only one guaranteed
    // to exist on the filesystem.  Therefore, it's the only safe
    // directory to point our daemon at.  I doubt this is strictly
    // necessary, but remembering to completely rebase a daemon is
    // part of just good hacking etiquette.
    if (0 > chdir("/")) {
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
int32_t make_socket()
{
    int32_t sock;
    sockaddr_in server;

    memset(static_cast<void*>(&server), 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (0 > (sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
        syslog(WARN, "couldn't create a server socket");
        exit(EXIT_FAILURE);
    }
    if (0 > bind(sock, reinterpret_cast<sockaddr*>(&server),
                 sizeof(server))) {
        syslog(WARN, "couldn't bind to port 9120");
        exit(EXIT_FAILURE);
    }
    if (0 > listen(sock, MAX_PENDING_REQUESTS)) {
        syslog(WARN, "couldn't listen for clients");
        exit(EXIT_FAILURE);
    }
    syslog(INFO, "ready for clients");
    return sock;
}

/** A thread that runs every thirty seconds checking to see if the
  * daemon should politely exit.  It will automatically shut down
  * if no clients are currently being serviced and more than
  * AUTOSHUTDOWN seconds have elapsed since the time the last client
  * connected. */
void* shutdown_handler(void*)
{
    while (1) {
        if (0 != pthread_mutex_lock(&mutex)) {
            syslog(WARN, "shutdown handler couldn't get mutex");
            exit(EXIT_FAILURE);
        }
        if (0 == active_sessions &&
                (TIMEOUT < (time(0) - last_req_at))) {
            syslog(INFO, "exiting normally due to inactivity");
            exit(EXIT_SUCCESS);
        }
        if (0 != pthread_mutex_unlock(&mutex)) {
            syslog(WARN, "shutdown handler couldn't release mutex");
            exit(EXIT_FAILURE);
        }
        sleep(30);
    }
    return NULL;
}

/** Checks a string to see if it's a valid base-10 number. */
int32_t is_num(const string& num)
{
    string::const_iterator b(num.begin());
    string::const_iterator e(num.end());

    return (e == find_if(b, e, not1(ptr_fun(::isdigit))))
           ? ::atoi(num.c_str())
           : -1;
}

/** Checks a string to see whether it's a port in the range
  * (1024, 65535) inclusive (i.e., in userspace). */
bool validate_port(const string& foo)
{
    PORT = is_num(foo) & 0xFFFF;
    return (PORT >= 1024);
}

bool validate_timeout(const string& foo)
{
    int32_t timeout(is_num(foo));
    if (0 == timeout) {
        timeout = INT_MAX;
    }
    else if (0 < timeout) {
        TIMEOUT = timeout;
    }
    return (0 < timeout);
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
         "-S : run as a normal process (do not run as a daemon)\n" <<
         "-o : only support old (1.0) nsrlsvr protocol\n" <<
         "-h : show this help message\n" <<
         "-p : listen on PORT, between 1024 and 65535 (default: 9120)\n" <<
         "-t : stop after TIMEOUT seconds of inactivity (default: disabled)\n\n";
    exit(EXIT_FAILURE);
}
}

/** An externally available const reference to the hash set. */
const set<string>& hashes(hash_set);

/** An externally available const reference to the variable storing
  * whether or not status checking should be enabled. */
const bool& enable_status(status_enabled);

/** An externally available const reference to the variable storing
  * whether or not only protocol 1.0 should be supported. */
const bool& only_old(old_only);

/** magic happens here */
int main(int argc, char* argv[])
{
    int32_t svr_sock(0);
    int32_t client_sock(0);
    sockaddr_in client;
    uint32_t client_length(0);
    pthread_t shutdown_handler_id;
    string port_num("9120");
    string timeout("0");
    std::auto_ptr<ifstream> infile;
    int32_t opt(0);

    while (-1 != (opt = getopt(argc, argv, "bsvof:hp:t:S"))) {
        switch (opt) {
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
            RDS_LOC = string((const char*) optarg);
            infile = std::auto_ptr<ifstream>(new ifstream(RDS_LOC.c_str()));
            if (not infile->good()) {
                cerr <<
                     "Error: the specified dataset file could not be found.\n\n";
                exit(EXIT_FAILURE);
            }
	    // No explicit close: the auto_ptr will take care of that
	    // on object destruction.
            break;
        case 'h':
            show_usage(argv[0]);
            break;
        case 'p':
            port_num = string(optarg);
            break;
        case 't':
            timeout = string(optarg);
            break;
        case 's':
            status_enabled = true;
            break;
        case 'S':
            standalone = true;
            break;
        case 'o':
            old_only = true;
            break;
        default:
            show_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (not (validate_port(port_num) and validate_timeout(timeout))) {
        show_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (not standalone)
        daemonize();

    load_hashes();
    svr_sock = make_socket();

    pthread_create(&shutdown_handler_id, NULL, shutdown_handler, NULL);

    while (true) {
        client_length = sizeof(client);
        if (0 > (client_sock = accept(svr_sock,
                                      reinterpret_cast<sockaddr*>(&client),
                                      &client_length))) {
            syslog(WARN, "dropped a connection");
        } else {
            try {
                pthread_t thread_id;
                const char* ipaddr(inet_ntoa(client.sin_addr));
                clientinfo* data(new clientinfo(client_sock, ipaddr));
                pthread_create(&thread_id, NULL, run_client_thread, data);
            } catch (std::bad_alloc&) {
                // There's no reason to have the server fall over:
                // the sysadmin might be able to kill off whatever
                // errant process is taking up all the RAM.
                syslog(WARN, "Critically short of available RAM!");
                continue;
            }
        }
    }
}
