#ifndef __MAIN_H
#define __MAIN_H

#include "../config.h"
#include <sys/types.h>
#include <string>
#include <utility>
#include <syslog.h>

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

void log(const LogLevel level, const std::string msg);
void handle_client(const int32_t);
pair64 to_pair64(std::string);
std::string from_pair64(pair64);

#endif

