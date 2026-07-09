#ifndef __LOGGER_HPP
#define __LOGGER_HPP

#include <stdarg.h>
#include <string>

namespace Logger {
#define COMPAT_LAYER_LOG_INFO (1ull << 0)
#define COMPAT_LAYER_LOG_ERROR (1ull << 1)

struct compat_layer_log {
    std::string name;
    unsigned long long value;
};

void log(const std::string &log_level, const char *format, ...);
} // namespace Logger

#endif
