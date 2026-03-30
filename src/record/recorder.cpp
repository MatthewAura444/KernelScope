/*
 * recorder.cpp — Event recording for KernelScope.
 *
 * Writes live events to a .ksr binary file for later replay.
 * Each event is length-prefixed, preceded by a KSR header.
 */

#include "record/recorder.hpp"

#include "record/format.hpp"
#include "util/logging.hpp"

namespace kscope {

Recorder::Recorder() = default;

Recorder::~Recorder() {
    close();
}

Status Recorder::open(const std::string& path) {
    file_ = std::fopen(path.c_str(), "wb");
    if (!file_)
        return std::unexpected(
            errno_error(ErrorCode::IoError, "failed to open " + path));

    auto hdr = make_header();
    if (std::fwrite(&hdr, sizeof(hdr), 1, file_) != 1)
        return std::unexpected(
            make_error(ErrorCode::IoError, "failed to write KSR header"));

    log::info("Recording to {}", path);
    return {};
}

Status Recorder::write_event(const Event& ev) {
    if (!file_)
        return std::unexpected(
            make_error(ErrorCode::Internal, "recorder not opened"));

    auto data = serialize_event(ev);
    uint32_t size = static_cast<uint32_t>(data.size());
    if (std::fwrite(&size, sizeof(size), 1, file_) != 1 ||
        std::fwrite(data.data(), 1, data.size(), file_) != data.size())
        return std::unexpected(
            make_error(ErrorCode::IoError, "fwrite failed during recording"));
    events_written_++;
    return {};
}

void Recorder::close() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
        log::info("Recording complete: {} events", events_written_);
    }
}

}
