/*
 * skeleton_bridge.hpp — Typed C++ wrapper around the BPF skeleton.
 *
 * Provides a higher-level interface over the raw skeleton struct,
 * abstracting away C map access and global variable pointers behind
 * a clean C++ API.  Used by the collector and diagnostics code.
 */

#pragma once

#include "bpf/loader.hpp"

#include <cstdint>
#include <vector>

namespace kscope {

class SkeletonBridge {
public:
    explicit SkeletonBridge(BpfLoader& loader);

    int ringbuf_fd() const;
    int stack_trace_fd() const;
    int active_syscalls_fd() const;
    int sched_states_fd() const;

    std::vector<uint64_t> read_stack(int32_t stack_id) const;

    uint32_t active_syscall_count() const;
    uint32_t sched_state_count() const;

private:
    BpfLoader& loader_;
};

}
