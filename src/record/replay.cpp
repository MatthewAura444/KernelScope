/*
 * replay.cpp — KSR file replay for KernelScope.
 *
 * Reads a .ksr recording and feeds events back through the aggregator,
 * allowing analysis without root or a live BPF session.
 */

#include "record/replay.hpp"

#include <cstdio>
#include <vector>

#include "record/format.hpp"
#include "util/logging.hpp"

namespace kscope {

ReplaySource::ReplaySource() = default;

ReplaySource::~ReplaySource() {
    close();
}

Status ReplaySource::open(const std::string& path) {
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_)
        return std::unexpected(
            errno_error(ErrorCode::IoError, "failed to open " + path));

    KsrHeader hdr;
    if (std::fread(&hdr, sizeof(hdr), 1, file_) != 1)
        return std::unexpected(
            make_error(ErrorCode::RecordFormatError, "failed to read KSR header"));

    if (!validate_header(hdr))
        return std::unexpected(
            make_error(ErrorCode::RecordFormatError, "invalid KSR file"));

    log::info("Opened KSR file: {}", path);
    return {};
}

Status ReplaySource::replay(EventCallback callback) {
    if (!file_)
        return std::unexpected(
            make_error(ErrorCode::Internal, "replay source not opened"));

    while (true) {
        uint32_t size;
        if (std::fread(&size, sizeof(size), 1, file_) != 1)
            break;

        if (size > 65536)
            return std::unexpected(
                make_error(ErrorCode::RecordFormatError, "event size exceeds 64KB limit"));

        std::vector<std::byte> buf(size);
        if (std::fread(buf.data(), 1, size, file_) != size)
            break;

        Event ev;
        if (deserialize_event(buf, ev)) {
            if (callback) callback(ev);
            events_replayed_++;
        }
    }

    log::info("Replayed {} events", events_replayed_);
    return {};
}

void ReplaySource::close() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

}
