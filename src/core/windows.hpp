/*
 * windows.hpp — Rolling time window management for KernelScope.
 *
 * Manages tumbling or sliding time windows for event aggregation.
 */

#pragma once

#include <cstdint>

namespace kscope {

class WindowManager {
public:
    explicit WindowManager(uint32_t window_sec = 5, uint32_t max_windows = 60);

    bool should_rotate(uint64_t current_ns) const;
    void rotate(uint64_t current_ns);
    void reset();

    uint64_t window_start_ns() const { return window_start_ns_; }
    uint32_t window_count() const { return window_count_; }

private:
    uint64_t window_duration_ns_;
    uint64_t window_start_ns_ = 0;
    uint32_t max_windows_;
    uint32_t window_count_ = 0;
};

}
