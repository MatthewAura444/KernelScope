/*
 * test_event_decoder.cpp — Unit tests for event decoding.
 *
 * Verifies that raw ring buffer bytes are correctly converted to
 * typed Event objects, including all field mappings and edge cases
 * like truncated or zero-length input.
 */

#include <gtest/gtest.h>

#include "core/event.hpp"
#include "core/event_decoder.hpp"
#include "events.h"

#include <cstring>
#include <vector>

namespace kscope {
namespace {

static struct event make_raw_event() {
    struct event raw{};
    raw.timestamp_ns   = 1234567890ULL;
    raw.pid            = 42;
    raw.tid            = 43;
    std::strncpy(raw.comm, "testproc", TASK_COMM_LEN);
    raw.cpu            = 3;
    raw.type           = EV_SYSCALL_SLOW;
    raw.classification = LAT_IO_WAIT;
    raw.duration_ns    = 5'000'000;
    raw.syscall_id     = 0;
    raw.user_stack_id  = 100;
    raw.kern_stack_id  = 200;
    raw.flags          = EVF_HAS_USER_STACK | EVF_HAS_KERN_STACK;
    return raw;
}

TEST(EventDecoder, DecodesValidEvent) {
    auto raw = make_raw_event();
    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(&raw), sizeof(raw));

    auto result = decode_event(span);
    ASSERT_TRUE(result.has_value());

    const auto& ev = *result;
    EXPECT_EQ(ev.timestamp_ns, 1234567890ULL);
    EXPECT_EQ(ev.pid, 42U);
    EXPECT_EQ(ev.tid, 43U);
    EXPECT_EQ(ev.comm_str(), "testproc");
    EXPECT_EQ(ev.cpu, 3U);
    EXPECT_EQ(ev.type, EventType::SyscallSlow);
    EXPECT_EQ(ev.classification, LatencyClass::IoWait);
    EXPECT_EQ(ev.duration_ns, 5'000'000ULL);
    EXPECT_EQ(ev.syscall_id, 0ULL);
    EXPECT_EQ(ev.user_stack_id, 100);
    EXPECT_EQ(ev.kern_stack_id, 200);
    EXPECT_TRUE(ev.has_user_stack());
    EXPECT_TRUE(ev.has_kern_stack());
}

TEST(EventDecoder, RejectsShortInput) {
    std::byte buf[4] = {};
    auto result = decode_event(std::span(buf, sizeof(buf)));
    EXPECT_FALSE(result.has_value());
}

TEST(EventDecoder, RejectsEmptyInput) {
    auto result = decode_event(std::span<const std::byte>{});
    EXPECT_FALSE(result.has_value());
}

TEST(EventDecoder, DecodesAllEventTypes) {
    struct { enum event_type raw; EventType expected; } cases[] = {
        {EV_NONE, EventType::None},
        {EV_SYSCALL_SLOW, EventType::SyscallSlow},
        {EV_OFFCPU_SLICE, EventType::OffcpuSlice},
        {EV_LOCK_WAIT, EventType::LockWait},
        {EV_IO_WAIT, EventType::IoWait},
        {EV_PROC_EXEC, EventType::ProcExec},
        {EV_PROC_EXIT, EventType::ProcExit},
        {EV_LOST_SAMPLES, EventType::LostSamples},
        {EV_DIAG, EventType::Diag},
    };

    for (auto& c : cases) {
        auto raw = make_raw_event();
        raw.type = c.raw;
        std::span<const std::byte> span(reinterpret_cast<const std::byte*>(&raw), sizeof(raw));
        auto result = decode_event(span);
        ASSERT_TRUE(result.has_value()) << "Failed for type " << static_cast<int>(c.raw);
        EXPECT_EQ(result->type, c.expected);
    }
}

TEST(EventDecoder, CommNulTerminatedCorrectly) {
    auto raw = make_raw_event();
    std::memset(raw.comm, 'A', TASK_COMM_LEN);
    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(&raw), sizeof(raw));

    auto result = decode_event(span);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->comm_str().size(), TASK_COMM_LEN);
}

TEST(EventDecoder, StackFlagsBitfield) {
    auto raw = make_raw_event();
    raw.flags = 0;
    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(&raw), sizeof(raw));

    auto result = decode_event(span);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_user_stack());
    EXPECT_FALSE(result->has_kern_stack());

    raw.flags = EVF_HAS_USER_STACK;
    result = decode_event(span);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_user_stack());
    EXPECT_FALSE(result->has_kern_stack());

    raw.flags = EVF_HAS_KERN_STACK;
    result = decode_event(span);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_user_stack());
    EXPECT_TRUE(result->has_kern_stack());
}

TEST(EventTypeName, KnownTypes) {
    EXPECT_EQ(event_type_name(EventType::SyscallSlow), "syscall_slow");
    EXPECT_EQ(event_type_name(EventType::OffcpuSlice), "offcpu_slice");
    EXPECT_EQ(event_type_name(EventType::LockWait), "lock_wait");
    EXPECT_EQ(event_type_name(EventType::IoWait), "io_wait");
    EXPECT_EQ(event_type_name(EventType::ProcExec), "proc_exec");
    EXPECT_EQ(event_type_name(EventType::ProcExit), "proc_exit");
}

TEST(LatencyClassName, KnownClasses) {
    EXPECT_EQ(latency_class_name(LatencyClass::OnCpu), "on_cpu");
    EXPECT_EQ(latency_class_name(LatencyClass::OffCpu), "off_cpu");
    EXPECT_EQ(latency_class_name(LatencyClass::IoWait), "io_wait");
    EXPECT_EQ(latency_class_name(LatencyClass::LockWait), "lock_wait");
    EXPECT_EQ(latency_class_name(LatencyClass::SyscallBlocked), "syscall_blocked");
}

TEST(SyscallName, KnownSyscalls) {
    EXPECT_EQ(syscall_name(0), "read");
    EXPECT_EQ(syscall_name(1), "write");
    EXPECT_EQ(syscall_name(2), "open");
    EXPECT_EQ(syscall_name(42), "connect");
    EXPECT_EQ(syscall_name(202), "futex");
    EXPECT_EQ(syscall_name(257), "openat");
}

TEST(SyscallName, UnknownFallback) {
    EXPECT_EQ(syscall_name(99999), "sys_99999");
}

}  // namespace
}  // namespace kscope
