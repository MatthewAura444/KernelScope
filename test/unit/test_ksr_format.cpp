/*
 * test_ksr_format.cpp — Unit tests for KSR binary format.
 *
 * Verifies header creation/validation, event serialization round-trips,
 * and rejection of invalid headers.
 */

#include <gtest/gtest.h>

#include "record/format.hpp"
#include "core/event.hpp"
#include "kscope/api.hpp"

#include <cstring>

namespace kscope {
namespace {

TEST(KsrHeader, MakeHeaderProducesValidHeader) {
    auto hdr = make_header();
    EXPECT_TRUE(validate_header(hdr));
    EXPECT_EQ(std::memcmp(hdr.magic, kKsrMagic.data(), 8), 0);
    EXPECT_EQ(hdr.version, kKsrVersion);
    EXPECT_GT(hdr.timebase_ns, 0ULL);
    EXPECT_GT(hdr.start_time_ns, 0ULL);
}

TEST(KsrHeader, InvalidMagicRejected) {
    auto hdr = make_header();
    hdr.magic[0] = 'X';
    EXPECT_FALSE(validate_header(hdr));
}

TEST(KsrHeader, FutureVersionRejected) {
    auto hdr = make_header();
    hdr.version = kKsrVersion + 1;
    EXPECT_FALSE(validate_header(hdr));
}

TEST(KsrHeader, HeaderSizeIs64Bytes) {
    EXPECT_EQ(sizeof(KsrHeader), 64U);
}

static Event make_test_event() {
    Event ev{};
    ev.timestamp_ns   = 1234567890ULL;
    ev.pid            = 42;
    ev.tid            = 43;
    std::strncpy(ev.comm.data(), "testproc", kCommLen);
    ev.cpu            = 3;
    ev.type           = EventType::SyscallSlow;
    ev.classification = LatencyClass::IoWait;
    ev.duration_ns    = 5'000'000;
    ev.syscall_id     = 0;
    ev.user_stack_id  = 100;
    ev.kern_stack_id  = 200;
    ev.flags          = 0x03;
    return ev;
}

TEST(KsrFormat, SerializeDeserializeRoundTrip) {
    auto original = make_test_event();
    auto bytes = serialize_event(original);
    EXPECT_GE(bytes.size(), sizeof(Event));

    Event decoded{};
    EXPECT_TRUE(deserialize_event(std::span(bytes), decoded));

    EXPECT_EQ(decoded.timestamp_ns, original.timestamp_ns);
    EXPECT_EQ(decoded.pid, original.pid);
    EXPECT_EQ(decoded.tid, original.tid);
    EXPECT_EQ(decoded.comm_str(), original.comm_str());
    EXPECT_EQ(decoded.cpu, original.cpu);
    EXPECT_EQ(decoded.type, original.type);
    EXPECT_EQ(decoded.classification, original.classification);
    EXPECT_EQ(decoded.duration_ns, original.duration_ns);
    EXPECT_EQ(decoded.syscall_id, original.syscall_id);
    EXPECT_EQ(decoded.user_stack_id, original.user_stack_id);
    EXPECT_EQ(decoded.kern_stack_id, original.kern_stack_id);
    EXPECT_EQ(decoded.flags, original.flags);
}

TEST(KsrFormat, DeserializeTruncatedFails) {
    auto original = make_test_event();
    auto bytes = serialize_event(original);
    auto truncated = std::span(bytes.data(), bytes.size() / 2);

    Event decoded{};
    EXPECT_FALSE(deserialize_event(truncated, decoded));
}

TEST(KsrFormat, DeserializeEmptyFails) {
    Event decoded{};
    EXPECT_FALSE(deserialize_event(std::span<const std::byte>{}, decoded));
}

TEST(KsrFormat, AllEventTypesRoundTrip) {
    EventType types[] = {
        EventType::None, EventType::SyscallSlow, EventType::OffcpuSlice,
        EventType::LockWait, EventType::IoWait, EventType::ProcExec,
        EventType::ProcExit, EventType::LostSamples, EventType::Diag,
    };

    for (auto t : types) {
        auto ev = make_test_event();
        ev.type = t;
        auto bytes = serialize_event(ev);
        Event decoded{};
        ASSERT_TRUE(deserialize_event(std::span(bytes), decoded));
        EXPECT_EQ(decoded.type, t);
    }
}

}  // namespace
}  // namespace kscope
