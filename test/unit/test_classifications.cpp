/*
 * test_classifications.cpp — Unit tests for latency classification.
 *
 * Validates the deterministic mapping from event attributes to latency
 * categories and the syscall group taxonomy.
 */

#include <gtest/gtest.h>

#include "core/classifications.hpp"
#include "core/event.hpp"

namespace kscope {
namespace {

static Event make_event(EventType type, uint64_t syscall_id = 0, uint64_t duration_ns = 1'000'000) {
    Event ev{};
    ev.type        = type;
    ev.syscall_id  = syscall_id;
    ev.duration_ns = duration_ns;
    ev.pid         = 1;
    ev.tid         = 1;
    return ev;
}

TEST(Classification, OffCpuSliceClassifiesAsOffCpu) {
    auto ev = make_event(EventType::OffcpuSlice);
    EXPECT_EQ(classify_event(ev), LatencyClass::OffCpu);
}

TEST(Classification, ReadWriteClassifyAsIoWait) {
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 0)), LatencyClass::IoWait);
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 1)), LatencyClass::IoWait);
}

TEST(Classification, FsyncClassifiesAsIoWait) {
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 74)), LatencyClass::IoWait);
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 75)), LatencyClass::IoWait);
}

TEST(Classification, NetworkSyscallsClassifyAsIoWait) {
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 42)), LatencyClass::IoWait);
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 43)), LatencyClass::IoWait);
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 44)), LatencyClass::IoWait);
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 45)), LatencyClass::IoWait);
}

TEST(Classification, FutexClassifiesAsLockWait) {
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 202)), LatencyClass::LockWait);
}

TEST(Classification, MemorySyscallsClassifyAsSyscallBlocked) {
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 9)), LatencyClass::SyscallBlocked);
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 12)), LatencyClass::SyscallBlocked);
}

TEST(Classification, ProcessSyscallsClassifyAsSyscallBlocked) {
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 56)), LatencyClass::SyscallBlocked);
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 59)), LatencyClass::SyscallBlocked);
}

TEST(Classification, UnknownSyscallClassifiesAsSyscallBlocked) {
    EXPECT_EQ(classify_event(make_event(EventType::SyscallSlow, 999)), LatencyClass::SyscallBlocked);
}

TEST(Classification, NoneEventReturnsUnknown) {
    EXPECT_EQ(classify_event(make_event(EventType::None)), LatencyClass::Unknown);
}

TEST(Classification, ProcExecReturnsUnknown) {
    EXPECT_EQ(classify_event(make_event(EventType::ProcExec)), LatencyClass::Unknown);
}

TEST(SyscallGroup, FileIoMapping) {
    EXPECT_EQ(syscall_group(0), SyscallGroup::FileIO);
    EXPECT_EQ(syscall_group(1), SyscallGroup::FileIO);
    EXPECT_EQ(syscall_group(2), SyscallGroup::FileIO);
    EXPECT_EQ(syscall_group(257), SyscallGroup::FileIO);
}

TEST(SyscallGroup, SyncMapping) {
    EXPECT_EQ(syscall_group(74), SyscallGroup::Sync);
    EXPECT_EQ(syscall_group(75), SyscallGroup::Sync);
}

TEST(SyscallGroup, NetworkMapping) {
    EXPECT_EQ(syscall_group(41), SyscallGroup::Network);
    EXPECT_EQ(syscall_group(42), SyscallGroup::Network);
    EXPECT_EQ(syscall_group(43), SyscallGroup::Network);
}

TEST(SyscallGroup, LockMapping) {
    EXPECT_EQ(syscall_group(202), SyscallGroup::Lock);
}

TEST(SyscallGroup, ProcessMapping) {
    EXPECT_EQ(syscall_group(56), SyscallGroup::Process);
    EXPECT_EQ(syscall_group(59), SyscallGroup::Process);
}

TEST(SyscallGroup, MemoryMapping) {
    EXPECT_EQ(syscall_group(9), SyscallGroup::Memory);
    EXPECT_EQ(syscall_group(11), SyscallGroup::Memory);
}

TEST(SyscallGroup, UnknownMapping) {
    EXPECT_EQ(syscall_group(999), SyscallGroup::Other);
}

}  // namespace
}  // namespace kscope
