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
#include <array>
#include <exception>
#include <inttypes.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <vector>

/* Additional defines necessary on Linux: */
#ifdef __linux__
#include <unistd.h> // because Fedora has lately taken to being weird
#endif

/* And Apple requires sys/uio.h for some reason. */
#ifdef __APPLE__
#include <sys/uio.h>
#include <unistd.h>
#endif

using std::string;
using std::find;
using std::find_if;
using std::transform;
using std::vector;
using std::remove;
using std::exception;
using std::binary_search;
using std::pair;
using std::copy;
using std::back_inserter;
using std::array;
using std::fill;

// defined in main.cc
extern const vector<pair64>& hashes;

namespace {
class NetworkTimeout : public std::exception {
public:
    virtual const char* what() const noexcept { return "network timeout"; }
};

class NetworkError : public std::exception {
public:
    virtual const char* what() const noexcept { return "network error"; }
};

string
read_line(const int32_t sockfd, int timeout = 15)
{
    static vector<char> buffer;
    static array<char, 8192> rdbuf;
    struct pollfd pfd;
    struct timeval start;
    struct timeval now;
    time_t elapsed_time;
    ssize_t bytes_received;
    constexpr auto MEGABYTE = 1 << 20;

    if (buffer.capacity() < MEGABYTE)
        buffer.reserve(MEGABYTE);

    // Step zero: check to see if there's already a string in the
    // input queue awaiting a read.
    auto iter = find(buffer.begin(), buffer.end(), '\n');
    if (iter != buffer.end()) {
        vector<char> newbuf(buffer.begin(), iter);
        buffer.erase(buffer.begin(), iter + 1);
        newbuf.erase(remove(newbuf.begin(), newbuf.end(), '\r'), newbuf.end());
        return string(newbuf.begin(), newbuf.end());
    }

    // Per POSIX, this can only err if we access invalid memory.
    // Since start is always valid, there's no problem here and
    // no need to check gettimeofday's return code.
    gettimeofday(&start, nullptr);
    now.tv_sec = start.tv_sec;
    now.tv_usec = start.tv_usec;
    elapsed_time = now.tv_sec - start.tv_sec;

    while ((elapsed_time < timeout)) {
        pfd.fd = sockfd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        fill(rdbuf.begin(), rdbuf.end(), 0);

        if ((buffer.size() > MEGABYTE) || (-1 == poll(&pfd, 1, 1000)) || (pfd.revents & POLLERR) || (pfd.revents & POLLHUP) || (pfd.revents & POLLNVAL)) {
            log(LogLevel::ALERT, "network error: ");
            if (buffer.size() > MEGABYTE) {
                log(LogLevel::ALERT, "buffer too large");
            }
            if (pfd.revents & POLLERR) {
                log(LogLevel::ALERT, "POLLERR");
            }
            if (pfd.revents & POLLHUP) {
                log(LogLevel::ALERT, "POLLHUP");
            }
            if (pfd.revents & POLLNVAL) {
                log(LogLevel::ALERT, "POLLNVAL");
            }
            throw NetworkError();
        }
        if (pfd.revents & POLLIN) {
            bytes_received = recvfrom(sockfd, &rdbuf[0], rdbuf.size(), 0, NULL, 0);
            if (0 == bytes_received) {
                log(LogLevel::ALERT, "read_line read on closed socket");
                throw NetworkError();
            }
            copy(rdbuf.begin(), rdbuf.begin() + bytes_received,
                back_inserter(buffer));
        }

        iter = find(buffer.begin(), buffer.end(), '\n');
        if (iter != buffer.end()) {
            string line(buffer.begin(), iter);
            if (line.at(line.size() - 1) == '\r') {
                line = string(line.begin(), line.end() - 1);
            }
            buffer.erase(buffer.begin(), iter + 1);
            return line;
        }
        gettimeofday(&now, nullptr);
        elapsed_time = now.tv_sec - start.tv_sec;
    }
    throw NetworkTimeout();
}

void write_line(const int32_t sockfd, string&& line)
{
    string output = line + "\r\n";
    const char* msg = output.c_str();
    if (-1 == send(sockfd, reinterpret_cast<const void*>(msg), output.size(), 0))
        throw NetworkError();
}

auto tokenize(string&& line, char character = ' ')
{
    vector<string> rv;
    transform(line.begin(), line.end(), line.begin(), toupper);

    auto begin = find_if(line.cbegin(), line.cend(), [&](auto x) { return x != character; });
    auto end = (begin != line.cend()) ? find(begin + 1, line.cend(), character)
                                      : line.cend();

    while (begin != line.cend()) {
        rv.emplace_back(string{ begin, end });
        if (end == line.cend()) {
            break;
        }
        begin = find_if(end + 1, line.cend(), [&](auto x) { return x != character; });
        end = (begin != line.cend()) ? find(begin + 1, line.cend(), character)
                                     : line.cend();
    }
    return rv;
}

string
generate_response(vector<string>::const_iterator begin,
    vector<string>::const_iterator end)
{
    string rv = "OK ";

    for (auto i = begin; i != end; ++i) {
        bool present = binary_search(hashes.cbegin(), hashes.cend(), to_pair64(*i));

        rv += present ? "1" : "0";
    }
    return rv;
}
}

void handle_client(const int32_t fd)
{
    enum class Command {
        Version = 0,
        Bye = 1,
        Status = 2,
        Query = 3,
        Upshift = 4,
        Downshift = 5,
        Unknown = 6
    };

    try {
        auto commands = tokenize(read_line(fd));
        while (true) {
            auto cmdstring = commands.at(0);
            Command cmd = Command::Unknown;

            if (cmdstring == "VERSION:")
                cmd = Command::Version;
            else if (cmdstring == "BYE")
                cmd = Command::Bye;
            else if (cmdstring == "STATUS")
                cmd = Command::Status;
            else if (cmdstring == "QUERY")
                cmd = Command::Query;
            else if (cmdstring == "UPSHIFT")
                cmd = Command::Upshift;
            else if (cmdstring == "DOWNSHIFT")
                cmd = Command::Downshift;

            switch (cmd) {
            case Command::Version:
                write_line(fd, "OK");
                break;

            case Command::Bye:
                return;

            case Command::Status:
                write_line(fd, "NOT SUPPORTED");
                break;

            case Command::Query:
                write_line(fd,
                    generate_response(commands.begin() + 1, commands.end()));
                break;

            case Command::Upshift:
                write_line(fd, "NOT OK");
                break;

            case Command::Downshift:
                write_line(fd, "NOT OK");
                break;

            case Command::Unknown:
                write_line(fd, "NOT OK");
                return;
            }
            commands = tokenize(read_line(fd));
        }
    } catch (std::exception&) {
        // Do nothing: just end the function, which will drop the connection.
    }
}
