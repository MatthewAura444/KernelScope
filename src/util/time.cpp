/*
 * time.cpp — Time formatting and clock implementation for KernelScope.
 */

#include "util/time.hpp"

#include <ctime>
#include <format>

namespace kscope::time {

std::string format_duration_ns(uint64_t ns) {
    if (ns < 1'000)
        return std::format("{}ns", ns);
    if (ns < 1'000'000)
        return std::format("{:.1f}µs", static_cast<double>(ns) / 1'000);
    if (ns < 1'000'000'000)
        return std::format("{:.2f}ms", static_cast<double>(ns) / 1'000'000);
    return std::format("{:.3f}s", static_cast<double>(ns) / 1'000'000'000);
}

std::string format_duration_us(uint64_t us) {
    return format_duration_ns(us * 1000);
}

std::string format_timestamp_ns(uint64_t ns) {
    uint64_t sec = ns / 1'000'000'000;
    uint64_t subsec_ms = (ns % 1'000'000'000) / 1'000'000;
    return std::format("{}.{:03d}", sec, subsec_ms);
}

uint64_t monotonic_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

uint64_t wall_clock_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

}
