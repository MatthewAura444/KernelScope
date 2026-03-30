/*
 * event.hpp — C++ event types mirroring BPF wire format for KernelScope.
 *
 * Provides the user-space representation of events received from the
 * BPF ring buffer.  Enums and structures here correspond 1:1 with
 * bpf/events.h but use proper C++ types and are in the kscope namespace.
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace kscope {

enum class EventType : uint32_t {
    None           = 0,
    SyscallSlow    = 1,
    OffcpuSlice    = 2,
    LockWait       = 3,
    IoWait         = 4,
    ProcExec       = 5,
    ProcExit       = 6,
    LostSamples    = 7,
    Diag           = 8,
};

enum class LatencyClass : uint32_t {
    Unknown        = 0,
    OnCpu          = 1,
    OffCpu         = 2,
    SyscallBlocked = 3,
    IoWait         = 4,
    LockWait       = 5,
    UnknownWait    = 6,
};

static constexpr size_t kCommLen = 16;

struct Event {
    uint64_t timestamp_ns;
    uint32_t pid;
    uint32_t tid;
    std::array<char, kCommLen> comm;
    uint32_t cpu;
    EventType type;
    LatencyClass classification;
    uint64_t duration_ns;
    uint64_t syscall_id;
    int64_t  user_stack_id;
    int64_t  kern_stack_id;
    uint32_t flags;

    std::string_view comm_str() const {
        return {comm.data(), strnlen(comm.data(), kCommLen)};
    }

    bool has_user_stack() const { return flags & (1U << 0); }
    bool has_kern_stack() const { return flags & (1U << 1); }
};

std::string_view event_type_name(EventType t);
std::string_view latency_class_name(LatencyClass c);
std::string syscall_name(uint64_t id);

}
