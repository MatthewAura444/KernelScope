/*
 * format.hpp — KSR binary recording format definition for KernelScope.
 *
 * Defines the on-disk layout of .ksr files: a fixed header, a stream
 * of length-prefixed serialized events, and an optional symbol cache
 * footer.
 */

#pragma once

#include "core/event.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace kscope {

struct KsrHeader {
    char     magic[8];
    uint32_t version;
    uint32_t flags;
    uint64_t timebase_ns;
    uint64_t start_time_ns;
    char     reserved[32];
};

static_assert(sizeof(KsrHeader) == 64, "KsrHeader must be exactly 64 bytes");

std::vector<std::byte> serialize_event(const Event& ev);
bool deserialize_event(std::span<const std::byte> data, Event& ev);

KsrHeader make_header();
bool validate_header(const KsrHeader& hdr);

}
