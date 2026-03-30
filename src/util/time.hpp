/*
 * time.hpp — Time formatting and monotonic clock helpers for KernelScope.
 *
 * Provides utilities to convert nanosecond timestamps to human-readable
 * strings, compute durations, and interface with the kernel's ktime
 * epoch used by BPF programs.
 */

#pragma once

#include <cstdint>
#include <string>

namespace kscope::time {

std::string format_duration_ns(uint64_t ns);
std::string format_duration_us(uint64_t us);
std::string format_timestamp_ns(uint64_t ns);

uint64_t monotonic_ns();
uint64_t wall_clock_ns();

inline constexpr uint64_t us_to_ns(uint64_t us) { return us * 1000; }
inline constexpr uint64_t ms_to_ns(uint64_t ms) { return ms * 1'000'000; }
inline constexpr uint64_t sec_to_ns(uint64_t s)  { return s * 1'000'000'000; }
inline constexpr uint64_t ns_to_us(uint64_t ns) { return ns / 1000; }
inline constexpr uint64_t ns_to_ms(uint64_t ns) { return ns / 1'000'000; }
inline constexpr uint64_t ns_to_sec(uint64_t ns) { return ns / 1'000'000'000; }

}
