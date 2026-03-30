/*
 * windows.cpp — Rolling window implementation for KernelScope.
 */

#include "core/windows.hpp"

namespace kscope {

WindowManager::WindowManager(uint32_t window_sec, uint32_t max_windows)
    : window_duration_ns_(static_cast<uint64_t>(window_sec) * 1'000'000'000)
    , max_windows_(max_windows) {}

bool WindowManager::should_rotate(uint64_t current_ns) const {
    if (window_start_ns_ == 0)
        return false;
    return (current_ns - window_start_ns_) >= window_duration_ns_;
}

void WindowManager::rotate(uint64_t current_ns) {
    window_start_ns_ = current_ns;
    window_count_++;
}

void WindowManager::reset() {
    window_start_ns_ = 0;
    window_count_ = 0;
}

}
