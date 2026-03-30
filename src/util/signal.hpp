/*
 * signal.hpp — Shared shutdown flag for KernelScope.
 *
 * Provides a global atomic flag that the signal handler (SIGINT/SIGTERM)
 * sets and that polling loops in command implementations check.
 */

#pragma once

#include <atomic>

namespace kscope {

inline std::atomic<bool> g_shutdown{false};

inline bool shutdown_requested() {
    return g_shutdown.load(std::memory_order_acquire);
}

inline void request_shutdown() {
    g_shutdown.store(true, std::memory_order_release);
}

}
