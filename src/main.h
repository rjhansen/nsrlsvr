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

#ifndef MAIN_H
#define MAIN_H

#include <string>
#include <sys/types.h>
#include <syslog.h>
#include <utility>

using ulong64 = unsigned long long;
using pair64 = std::pair<ulong64, ulong64>;

enum class LogLevel
{
  INFO = LOG_INFO,
  WARN = LOG_WARNING,
  DEBUG = LOG_DEBUG,
  CRITICAL = LOG_CRIT,
  ALERT = LOG_ALERT,
  EMERGENCY = LOG_EMERG
};

void log(const LogLevel, const std::string&&);
void handle_client(const int32_t);
pair64 to_pair64(std::string);
std::string from_pair64(pair64);
bool operator<(const pair64& lhs, const pair64& rhs);
bool operator==(const pair64& lhs, const pair64& rhs);
bool operator>(const pair64& lhs, const pair64& rhs);

#endif
