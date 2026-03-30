/*
 * logging.hpp — Lightweight stderr logging for KernelScope.
 *
 * Provides leveled logging (debug, info, warn, error) with optional
 * color output.  The global log level is set once at startup and
 * controls which messages are emitted.  Thread-safe via stderr.
 */

#pragma once

#include <cstdint>
#include <format>
#include <string_view>

namespace kscope::log {

enum class Level : uint8_t {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
    Off   = 4,
};

void set_level(Level lvl);
Level get_level();
void set_color(bool enabled);

void emit(Level lvl, std::string_view msg);

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    if (get_level() <= Level::Debug)
        emit(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    if (get_level() <= Level::Info)
        emit(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    if (get_level() <= Level::Warn)
        emit(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    if (get_level() <= Level::Error)
        emit(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

}
