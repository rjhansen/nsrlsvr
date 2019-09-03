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

#ifndef MAIN_H
#define MAIN_H

#include <syslog.h>
#include <boost/asio.hpp>
#include <cstdint>
#include <string>
#include <utility>

// Note: C++11 guarantees an unsigned long long will be at least 64 bits.
// A compile-time assert in main.cc guarantees it will ONLY be 64 bits.
using pair64 = std::pair<unsigned long long, unsigned long long>;

enum class LogLevel {
  INFO = LOG_INFO,
  WARN = LOG_WARNING,
  DEBUG = LOG_DEBUG,
  CRITICAL = LOG_CRIT,
  ALERT = LOG_ALERT,
  EMERGENCY = LOG_EMERG
};

void log(const LogLevel, const std::string&&);
void handle_client(boost::asio::ip::tcp::iostream& stream);
pair64 to_pair64(const std::string&);
std::string from_pair64(const pair64&);
bool operator<(const pair64& lhs, const pair64& rhs);
bool operator==(const pair64& lhs, const pair64& rhs);
bool operator>(const pair64& lhs, const pair64& rhs);

#endif
