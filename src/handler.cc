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
 *
 * Robert J. Hansen <rjh@secret-alchemy.com>
 *   - most everything
 * Jesse Kornblum <jessekornblum@gmail.com>
 *   - patch to log how many hashes are in each QUERY statement
 */

#include <string>
#include <set>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>
#include <exception>
#include "handler.hpp"
#include <poll.h>
#include <cstdlib> // for getloadavg
#include <sys/types.h>
#include <syslog.h>
#include <inttypes.h>
#include <utility>

#define INFO LOG_MAKEPRI(LOG_USER, LOG_INFO)

/* Additional defines necessary on Linux: */
#ifdef __linux__
#include <cstring> // for memset
#include <cstdio> // for snprintf
#include <unistd.h> // because Fedora has lately taken to being weird
#endif

/* And Apple requires sys/uio.h for some reason. */
#ifdef __APPLE__
#include <sys/uio.h>
#include <unistd.h>
#endif


using std::set;
using std::string;
using std::find;
using std::find_if;
using std::transform;
using std::vector;
using std::not1;
using std::equal_to;
using std::ptr_fun;
using std::remove;
using std::auto_ptr;
using std::exception;
using std::binary_search;
using std::pair;

typedef unsigned long long ULONG64;
typedef pair<ULONG64, ULONG64> pair64;

extern const vector<pair64>& hashes;
extern const bool& enable_status;
extern const bool& only_old;

namespace
{

/** A convenience exception representing network errors that cannot
  * be recovered from, and will result in a graceful bomb-out.
  *
  * @since 1.1
  * @author Robert J. Hansen */
class UnrecoverableNetworkError : public exception
{
public:
    const char* what() const throw()
    {
        return "unr net err";
    }
};


/** A functor that provides stateful reading of line-oriented data
  * across UNIX file descriptors.
  *
  * The big problem with reading information over a socket
  * connection is that data can arrive in a badly fragmented form.
  * On a console you can just call getline() and be confident that
  * when it returns there will be a CR/LF at the end and no data
  * afterwards: that's the great virtue of accepting data one byte
  * at a time on a tty.  On a network connection you have to take
  * what the system gives you, and if the system gives you two
  * strings spread over three packets with a CR/LF smack in the
  * middle, well ... you have to make do.  That means returning the
  * first line and storing the rest of the data for use in a
  * subsequent call to the data reading facility.
  *
  * So, in other words, our get_line function needs to track state
  * *and* be threadsafe/re-entrant.  Declaring a static buffer within
  * the function would let it track state, but thread safety would be
  * a problem.
  *
  * Fortunately, the C++ functor idiom solves this problem
  * beautifully.
  *
  * Further: naïve blocking I/O, although it works rather well, will
  * artificially inflate the server load.  For this reason the code
  * uses slightly more complex but still quite manageable poll()-
  * based I/O with a 750ms timeout.  Responsiveness isn't quite as
  * high as it could be, but it's a small price to pay for better
  * behavior server-side.
  *
  * @author Rob Hansen
  * @since 0.9*/

struct SocketIO
{
public:
    /** Initializes the object to listen on a particular file
      * descriptor.
      *
      * @param fd File descriptor to read on */
    SocketIO(int32_t fd) :
        sock_fd(fd), buffer(""), tmp_buf(65536, '\0') {}

    /** Writes a line of text to the socket.  The caller is
      * responsible for ensuring the text has a '\r\n' appended.
      *
      * @param line The line to write
      * @since 1.1 */
    void write_line(string line) const
    {
        if (-1 == write(sock_fd, line.c_str(), line.size()))
        {
            throw UnrecoverableNetworkError();
        }
    }

    /** Writes a line of text to the socket.  The caller is
      * responsible for ensuring the text has a '\r\n' appended.
      *
      * @param line The line to write
      * @since 1.1 */
    void write_line(const char* line) const
    {
        write_line(string(line));
    }

    /** Reads a line from the socket.  Returns an auto_ptr<string>
      * because clients might be sending arbitrarily-sized (i.e.,
      * really huge) data to us.  Passing smartpointers around is
      * ridiculously faster than copying huge blocks of memory.
      *
      * Arguably this should return a shared_ptr<string>, but a lot
      * of C++ compilers have shaky support for TR1.  Instead we use
      * the lowest common denominator: std::auto_ptr.
      *
      * This function replaces the old operator().
      *
      * @since 1.1
      * @return An auto_ptr<string> representing one line read from
      * the file descriptor.*/
    auto_ptr<string> read_line()
    {
        /* "But in Latin, Jehovah begins with the letter 'I'..."
         *
         * SAVE YOURSELF THE NIGHTMARE BUG HUNT.  Remember that when
         * you test this code at the console, tapping return will
         * enter a \n.  When you do it from a Telnet client, it enters
         * a \r\n.  This one-character difference turned into a six-
         * hour bug hunt.  Documented here for posterity.  If you ever
         * wonder why I'm tempted to start drinking before the sun
         * rises, well, this one's a good example... */

        while (true)
        {
            pollfd fds = { sock_fd, POLLIN, 0 };
            int poll_code(poll(&fds, 1, 750));

            if (-1 == poll_code)
                throw UnrecoverableNetworkError();

            else if (fds.revents & POLLERR ||
                     fds.revents & POLLHUP)
                throw UnrecoverableNetworkError();

            else if (fds.revents & POLLIN)
            {
                memset(static_cast<void*>(&tmp_buf[0]),
                       0,
                       tmp_buf.size());
                ssize_t bytes_read = read(sock_fd,
                                          static_cast<void*>(&tmp_buf[0]),
                                          tmp_buf.size());
                buffer += string(&tmp_buf[0], &tmp_buf[bytes_read]);

                /* To prevent DoS from clients spamming us with huge
                   packets, bomb on any query larger than 256k. */
                if (buffer.size() > 262144)
                    throw UnrecoverableNetworkError();

                string::iterator iter = find(buffer.begin(),
                                             buffer.end(), '\n');
                if (iter != buffer.end())
                {
                    auto_ptr<string> rv(new string(buffer.begin(), iter));
                    rv->erase(remove(rv->begin(),
                                     rv->end(),
                                     '\r'),
                              rv->end());
                    rv->erase(remove(rv->begin(),
                                     rv->end(),
                                     '\n'),
                              rv->end());
                    buffer = string(iter + 1, buffer.end());
                    return rv;
                }
            }
        }
    }
private:
    /** Tracks the file descriptor to read */
    const int32_t sock_fd;
    /** Internal storage buffer for keeping track of read, but not
      * yet finished, data */
    string buffer;
    /** Internal storage buffer used only briefly, but declared here
      * in order so that we can avoid repeatedly putting it on the
      * stack.  Additionally, this only takes a few bytes on the stack:
      * the actual buffer gets allocated on the heap. */
    vector<char> tmp_buf;
};

/** A hand-rolled string tokenizer in C++.
  *
  * Efficient string tokenization in 29 lines, without absurd
  * contortions of code.  Booyah.  Given the state of things in C,
  * where on some platforms strtok is outright obsoleted by strsep
  * and on other platforms strsep is just a distant promise of what
  * the future might hold... I'll take this way.
  *
  * Returns a smartpointer to a vector for the same reason
  * SocketIO::read_line() returns one: to spare us the
  * otherwise absurd amount of memcpying that would be going on.
  *
  * @param line A pointer to the line to tokenize
  * @param character The delimiter character
  * @returns An auto_ptr to a vector of strings representing tokens */
auto_ptr<vector<string> > tokenize(string& line, char character = ' ')
{
    auto_ptr<vector<string> > rv(new vector<string>());
    transform(line.begin(), line.end(), line.begin(), toupper);

    string::iterator begin(find_if(line.begin(), line.end(),
                                   not1(bind2nd(equal_to<char>(),
                                           character))));
    string::iterator end(
        (begin != line.end())
        ? find(begin + 1, line.end(), character)
        : line.end()
    );

    while (begin != line.end())
    {
        rv->push_back(string(begin, end));
        if (end == line.end())
        {
            begin = line.end();
            continue;
        }
        begin = find_if(end + 1, line.end(),
                        not1(bind2nd(equal_to<char>(), character)));
        end = (begin != line.end())
              ? find(begin + 1, line.end(), character)
              : line.end();
    }
    return rv;
}

/** A hand-rolled string tokenizer in C++.
  *
  * Efficient string tokenization in 29 lines, without absurd
  * contortions of code.  Booyah.  Given the state of things in C,
  * where on some platforms strtok is outright obsoleted by strsep
  * and on other platforms strsep is just a distant promise of what
  * the future might hold... I'll take this way.
  *
  * Returns a smartpointer to a vector for the same reason
  * SocketIO::read_line() returns one: to spare us the
  * otherwise absurd amount of memcpying that would be going on.
  *
  * @param line A pointer to the line to tokenize
  * @param character The delimiter character
  * @returns An auto_ptr to a vector of strings representing tokens */
auto_ptr<vector<string> > tokenize(auto_ptr<string> line, char ch = ' ')
{
    return tokenize(*line, ch);
}

/** Turns a string of 'a.b.c.d', ala dotted-quad style, into a
  * 32-bit integer.  'a' must be present: if b through d are
  * omitted, they are assumed to be zero.
  *
  * @param line A smartpointer to a version string
  * @returns A 32-bit integer representing a version, or -1 on
  * failure.
  * @author Rob Hansen
  * @since 0.9 */
int32_t parse_version(auto_ptr<string> line)
{
    int32_t version(0);
    int32_t this_token(0);
    auto_ptr<vector<string> > tokens(tokenize(line));
    auto_ptr<vector<string> > version_tokens;
    size_t index(0);

    if (tokens->size() != 2 or
            tokens->at(0) != "VERSION:")
    {
        goto PARSE_VERSION_BAIL_BAD;
    }

    version_tokens = tokenize(tokens->at(1), '.');

    if (version_tokens->size() < 1 or version_tokens->size() > 4)
    {
        goto PARSE_VERSION_BAIL_BAD;
    }

    while (version_tokens->size() != 4)
    {
        version_tokens->push_back("0");
    }

    for (index = 0 ; index < 4 ; ++index)
    {
        string& thing(version_tokens->at(index));
        if (thing.end() != find_if(thing.begin(),
                                   thing.end(),
                                   not1(ptr_fun(::isdigit))))
        {
            goto PARSE_VERSION_BAIL_BAD;
        }
        this_token = atoi(thing.c_str());
        if (this_token < 0 or this_token > 254)
        {
            goto PARSE_VERSION_BAIL_BAD;
        }
        version = (version << 8) + this_token;
    }
    goto PARSE_VERSION_BAIL;

PARSE_VERSION_BAIL_BAD:
    version = -1;

PARSE_VERSION_BAIL:
    return version;
}


/** A simple convenience function that allows us to ensure
  * we're getting valid hashes.
  *
  * @param digest The string being checked
  * @returns true if it could be an MD5 or SHA-1 digest, false otherwise
  * @since 0.9
  * @author Rob Hansen */
bool ishexdigest(const string& digest)
{
    string::const_iterator iter(digest.begin());

    if (not (digest.size() == 40 or digest.size() == 32))
    {
        return false;
    }
    for ( ; iter != digest.end() ; ++iter)
    {
        bool is_number = (*iter >= '0' and *iter <= '9');
        bool is_letter = (*iter >= 'A' and *iter <= 'F');
        if (not (is_number or is_letter))
            return false;
    }
    return true;
}

/** Performs a transaction with a client.  Adheres to protocol
  * version 1.0.
  *
  * @param sio The socket to listen and respond on
  * @param ip_addr The IP address of the remote host
  * @since 0.9 */
void handle_protocol_10(SocketIO& sio, const char* ip_addr)
{
    string return_seq("");
    uint32_t found(0);
    double frac(0.0);
    uint32_t total_queries(0);

    try
    {
        auto_ptr<vector<string> > commands(tokenize(sio.read_line()));

        if (commands->size() < 2 or commands->at(0) != "QUERY")
        {
            sio.write_line("NOT OK\r\n");
            return;
        }

        for (size_t index = 1 ; index < commands->size() ; ++index)
        {
            if (not ishexdigest(commands->at(index)))
            {
                sio.write_line("NOT OK\r\n");
                return;
            }
            if (binary_search(hashes.begin(), hashes.end(),
                              to_pair64(commands->at(index))))
            {
                return_seq += "1";
                found += 1;
            }
            else
            {
                return_seq += "0";
            }
        }

        total_queries = commands->size() -
                        (commands->size() > 0 ? 1 : 0);

        if (total_queries)
        {
            double numerator(100 * found);
            double denominator(total_queries);
            frac = numerator / denominator;
        }

        syslog(INFO,
               "%s: protocol 1.0, found %u of %u hashes (%.1f%%), closed normally",
               ip_addr,
               found,
               total_queries,
               frac);
        return_seq = "OK " + return_seq + "\r\n";
        sio.write_line(return_seq);
    }
    catch (exception&)
    {
        return;
    }
}

/** Performs a transaction with a client.  Adheres to protocol
  * version 2.0.
  *
  * @param sio The socket to listen and respond on
  * @since 1.1 */
void handle_protocol_20(SocketIO& sio, const char* ip_addr)
{
    uint32_t total_queries(0);
    uint32_t found(0);
    double frac(0.0);

    try
    {
        auto_ptr<vector<string> > commands(tokenize(sio.read_line()));
        while (commands->size() >= 1)
        {
            string return_seq("");

            if ("BYE" == commands->at(0))
            {
                if (total_queries)
                {
                    double numerator(100 * found);
                    double denominator(total_queries);
                    frac = numerator / denominator;
                }
                syslog(INFO,
                       "%s: protocol 2.0, found %u of %u hashes (%.1f%%), closed normally",
                       ip_addr,
                       found,
                       total_queries,
                       frac);
                return;
            }

            else if ("DOWNSHIFT" == commands->at(0))
            {
                syslog(INFO,
                       "%s asked for a protocol downgrade to 1.0",
                       ip_addr);
                sio.write_line("OK\r\n");
                handle_protocol_10(sio, ip_addr);
                return;
            }

            else if ("UPSHIFT" == commands->at(0))
            {
                syslog(INFO,
                       "%s asked for a protocol upgrade (refused)",
                       ip_addr);
                sio.write_line("NOT OK\r\n");
            }

            else if ("QUERY" == commands->at(0))
            {
                if (commands->size() == 1)
                {
                    sio.write_line("NOT OK\r\n");
                    return;
                }
                else
                {
                    size_t index(1);
                    for ( ; index < commands->size() ; ++index)
                    {
                        if (not ishexdigest(commands->at(index)))
                        {
                            sio.write_line("NOT OK\r\n");
                            return;
                        }
                        if (binary_search(hashes.begin(), hashes.end(),
                                          to_pair64(commands->at(index))))
                        {
                            return_seq += "1";
                            found += 1;
                        }
                        else
                        {
                            return_seq += "0";
                        }
                    }
                    return_seq = "OK " + return_seq + "\r\n";
                    total_queries += commands->size() - 1;
                }
            }

            else if ("STATUS" == commands->at(0) and enable_status)
            {
                double loadavg[3] = { 0.0, 0.0, 0.0 };
                char buf[1024];

                getloadavg(loadavg, 3);
                memset(buf, 0, 1024);
                snprintf(buf,
                         1024,
                         "OK %u %s hashes, load %.2f %.2f %.2f\r\n",
                         (u_int32_t) hashes.size(),
                         "MD5",
                         loadavg[0],
                         loadavg[1],
                         loadavg[2]);
                string line(buf);
                return_seq = string(buf);
                syslog(INFO,
                       "%s asked for server status (sent '%s')",
                       ip_addr,
                       buf);
            }
            else if ("STATUS" == commands->at(0))
            {
                syslog(INFO,
                       "%s asked for server status (refused)",
                       ip_addr);
                return_seq = "OK NOT SUPPORTED\r\n";
            }
            else
            {
                sio.write_line("NOT OK\r\n");
                return;
            }
            sio.write_line(return_seq);
            commands = tokenize(sio.read_line());
        }
    }
    catch (exception&)
    {
        if (total_queries)
        {
            double numerator(100 * found);
            double denominator(total_queries);
            frac = numerator / denominator;
        }
        syslog(INFO,
               "%s: protocol 2.0, found %u of %u hashes (%.1f%%), closed abnormally",
               ip_addr,
               found,
               total_queries,
               frac);
    }
}
}

/** Handles client query requests.
  *
  * @param fd the client's socket file descriptor
  * @since 0.9 */
void handle_client(const int32_t fd, const string& ip_addr)
{
    SocketIO sio(fd);

    try
    {
        int32_t version(parse_version(sio.read_line()));
        if (version > 0 and version <= 0x01000000)
        {
            sio.write_line("OK\r\n");
            handle_protocol_10(sio, ip_addr.c_str());
        }
        else if (version > 0x01000000 and
                 version <= 0x02000000 and
                 not only_old)
        {
            sio.write_line("OK\r\n");
            handle_protocol_20(sio, ip_addr.c_str());
        }
        else
        {
            sio.write_line("NOT OK\r\n");
        }
    }
    catch (exception&)
    {
        return;
    }
}
