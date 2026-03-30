/*
 * format.cpp — KSR format serialization for KernelScope.
 *
 * Handles serialization and deserialization of Event structs to/from
 * the .ksr binary recording format.  Uses direct memcpy since Event
 * is a trivially-copyable struct and .ksr files are same-architecture.
 */

#include "record/format.hpp"

#include <cstring>

#include "kscope/api.hpp"
#include "util/time.hpp"

namespace kscope {

KsrHeader make_header() {
    KsrHeader hdr{};
    std::memcpy(hdr.magic, kKsrMagic.data(), 8);
    hdr.version = kKsrVersion;
    hdr.flags = 0;
    hdr.timebase_ns = time::monotonic_ns();
    hdr.start_time_ns = time::wall_clock_ns();
    std::memset(hdr.reserved, 0, sizeof(hdr.reserved));
    return hdr;
}

bool validate_header(const KsrHeader& hdr) {
    return std::memcmp(hdr.magic, kKsrMagic.data(), 8) == 0 &&
           hdr.version <= kKsrVersion;
}

std::vector<std::byte> serialize_event(const Event& ev) {
    std::vector<std::byte> buf(sizeof(Event));
    std::memcpy(buf.data(), &ev, sizeof(Event));
    return buf;
}

bool deserialize_event(std::span<const std::byte> data, Event& ev) {
    if (data.size() < sizeof(Event))
        return false;
    std::memcpy(&ev, data.data(), sizeof(Event));
    return true;
}

}
