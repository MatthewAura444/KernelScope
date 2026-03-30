/*
 * recorder.hpp — Live event recording to .ksr files for KernelScope.
 *
 * Writes a stream of events to disk in append-only KSR format.
 */

#pragma once

#include "core/event.hpp"
#include "util/errors.hpp"

#include <cstdio>
#include <string>

namespace kscope {

class Recorder {
public:
    Recorder();
    ~Recorder();

    Status open(const std::string& path);
    Status write_event(const Event& ev);
    void close();

    uint64_t events_written() const { return events_written_; }

private:
    FILE* file_ = nullptr;
    uint64_t events_written_ = 0;
};

}
