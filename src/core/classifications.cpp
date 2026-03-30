/*
 * classifications.cpp — Latency classification for KernelScope.
 *
 * Classifies slow events into latency categories (IO, lock, off-CPU, etc.)
 * based on syscall group and event type.
 */

#include "core/classifications.hpp"

namespace kscope {

SyscallGroup syscall_group(uint64_t id) {
    switch (id) {
        case 0: case 1: case 2: case 3: case 5: case 8:
        case 17: case 18: case 257: case 262:
            return SyscallGroup::FileIO;
        case 74: case 75:
            return SyscallGroup::Sync;
        case 41: case 42: case 43: case 44: case 45: case 46:
        case 47: case 49: case 50: case 288:
            return SyscallGroup::Network;
        case 202:
            return SyscallGroup::Lock;
        case 56: case 57: case 59: case 61:
            return SyscallGroup::Process;
        case 9: case 10: case 11: case 12:
            return SyscallGroup::Memory;
        default:
            return SyscallGroup::Other;
    }
}

LatencyClass classify_event(const Event& ev) {
    if (ev.type == EventType::OffcpuSlice)
        return LatencyClass::OffCpu;

    if (ev.type == EventType::SyscallSlow) {
        auto group = syscall_group(ev.syscall_id);
        switch (group) {
            case SyscallGroup::Lock:
                return LatencyClass::LockWait;
            case SyscallGroup::FileIO:
            case SyscallGroup::Sync:
            case SyscallGroup::Network:
                return LatencyClass::IoWait;
            default:
                return LatencyClass::SyscallBlocked;
        }
    }

    return LatencyClass::Unknown;
}

}
