/*
 * skeleton_bridge.cpp — C++ wrapper around BPF skeleton for KernelScope.
 */

#include "bpf/skeleton_bridge.hpp"

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <cstring>

#include "events.h"
#include "kscope.skel.h"

namespace kscope {

SkeletonBridge::SkeletonBridge(BpfLoader& loader) : loader_(loader) {}

int SkeletonBridge::ringbuf_fd() const {
    return loader_.ringbuf_fd();
}

int SkeletonBridge::stack_trace_fd() const {
    return loader_.stack_trace_fd();
}

int SkeletonBridge::active_syscalls_fd() const {
    auto* skel = loader_.skeleton();
    if (!skel) return -1;
    return bpf_map__fd(skel->maps.active_syscalls);
}

int SkeletonBridge::sched_states_fd() const {
    auto* skel = loader_.skeleton();
    if (!skel) return -1;
    return bpf_map__fd(skel->maps.sched_states);
}

std::vector<uint64_t> SkeletonBridge::read_stack(int32_t stack_id) const {
    std::vector<uint64_t> addrs(MAX_STACK_DEPTH, 0);
    int fd = stack_trace_fd();
    if (fd < 0 || stack_id < 0)
        return {};

    uint32_t key = static_cast<uint32_t>(stack_id);
    int err = bpf_map_lookup_elem(fd, &key, addrs.data());
    if (err) return {};

    while (!addrs.empty() && addrs.back() == 0)
        addrs.pop_back();

    return addrs;
}

uint32_t SkeletonBridge::active_syscall_count() const {
    int fd = active_syscalls_fd();
    if (fd < 0) return 0;

    struct task_key key = {}, next_key = {};
    uint32_t count = 0;
    while (bpf_map_get_next_key(fd, &key, &next_key) == 0) {
        count++;
        key = next_key;
    }
    return count;
}

uint32_t SkeletonBridge::sched_state_count() const {
    int fd = sched_states_fd();
    if (fd < 0) return 0;

    struct task_key key = {}, next_key = {};
    uint32_t count = 0;
    while (bpf_map_get_next_key(fd, &key, &next_key) == 0) {
        count++;
        key = next_key;
    }
    return count;
}

}
