#include "logger.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>

namespace Logger {
static struct compat_layer_log compat_layer_log_options[] = {
    {"info", COMPAT_LAYER_LOG_INFO},
    {"error", COMPAT_LAYER_LOG_ERROR},
    {"", 0}};

uint64_t compat_layer_log_mask;

void init();

static unsigned long long get_debug_flag(const char *option) {
    int index = 0;

    while (!compat_layer_log_options[index].name.empty()) {
        if (!std::strcmp(compat_layer_log_options[index].name.c_str(), option))
            return compat_layer_log_options[index].value;

        index++;
    }

    return 0;
}

static bool get_compat_layer_log_level(const std::string &option) {
    uint64_t flag = get_debug_flag(option.c_str());

    if (compat_layer_log_mask & flag)
        return true;

    return false;
}

void log(const std::string &log_level, const char *format, ...) {
    Logger::init();
    // if (!get_compat_layer_log_level(log_level.c_str()))
    // 	return;

    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, BUFFER_SIZE, format, args);
    va_end(args);

    std::cerr << "[" << log_level << "]: " << buffer << std::endl;
}

void init() {
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    char *compat_layer_log_env = std::getenv("COMPAT_LOG_LEVEL");
    if (!compat_layer_log_env) {
        compat_layer_log_mask = 0;
        return;
    }

    const char *option = std::strtok(compat_layer_log_env, ",");

    while (option != nullptr) {
        compat_layer_log_mask |= get_debug_flag(option);
        option = std::strtok(nullptr, ",");
    }
}
} // namespace Logger
