/*
 * classifications.hpp — Latency classification engine for KernelScope.
 *
 * Deterministic mapping from event attributes to latency categories:
 * on_cpu, off_cpu, syscall_blocked, io_wait, lock_wait, unknown_wait.
 * Syscall group taxonomy and futex/lock heuristics live here.
 */

#pragma once

#include "core/event.hpp"

namespace kscope {

LatencyClass classify_event(const Event& ev);

enum class SyscallGroup {
    FileIO,
    Sync,
    Network,
    Lock,
    Process,
    Memory,
    Signal,
    Other,
};

SyscallGroup syscall_group(uint64_t syscall_id);

}
