/*
 * logging.cpp — Implementation of stderr logging for KernelScope.
 */

#include "util/logging.hpp"

#include <atomic>
#include <cstdio>
#include <ctime>

namespace kscope::log {

namespace {
std::atomic<Level> g_level{Level::Info};
std::atomic<bool>  g_color{true};

constexpr const char* level_str(Level lvl) {
    switch (lvl) {
        case Level::Debug: return "DBG";
        case Level::Info:  return "INF";
        case Level::Warn:  return "WRN";
        case Level::Error: return "ERR";
        default:           return "???";
    }
}

constexpr const char* level_color(Level lvl) {
    switch (lvl) {
        case Level::Debug: return "\033[90m";
        case Level::Info:  return "\033[36m";
        case Level::Warn:  return "\033[33m";
        case Level::Error: return "\033[31m";
        default:           return "";
    }
}

constexpr const char* kReset = "\033[0m";
}

void set_level(Level lvl) { g_level.store(lvl, std::memory_order_relaxed); }
Level get_level()         { return g_level.load(std::memory_order_relaxed); }
void set_color(bool on)   { g_color.store(on, std::memory_order_relaxed); }

void emit(Level lvl, std::string_view msg) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    bool color = g_color.load(std::memory_order_relaxed);
    if (color) {
        std::fprintf(stderr, "%s[%02d:%02d:%02d.%03ld %s]%s %.*s\n",
                     level_color(lvl),
                     tm.tm_hour, tm.tm_min, tm.tm_sec,
                     ts.tv_nsec / 1000000,
                     level_str(lvl),
                     kReset,
                     static_cast<int>(msg.size()), msg.data());
    } else {
        std::fprintf(stderr, "[%02d:%02d:%02d.%03ld %s] %.*s\n",
                     tm.tm_hour, tm.tm_min, tm.tm_sec,
                     ts.tv_nsec / 1000000,
                     level_str(lvl),
                     static_cast<int>(msg.size()), msg.data());
    }
}

}
