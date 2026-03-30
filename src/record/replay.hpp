/*
 * replay.hpp — KSR file replay for KernelScope.
 *
 * Reads a .ksr file and emits events through the same callback
 * interface as the live Collector, enabling all downstream processing
 * (aggregation, export, TUI) to work identically on recorded data.
 */

#pragma once

#include "core/event.hpp"
#include "util/errors.hpp"

#include <functional>
#include <string>

namespace kscope {

using EventCallback = std::function<void(const Event&)>;

class ReplaySource {
public:
    ReplaySource();
    ~ReplaySource();

    Status open(const std::string& path);
    Status replay(EventCallback callback);
    void close();

    uint64_t events_replayed() const { return events_replayed_; }

private:
    FILE* file_ = nullptr;
    uint64_t events_replayed_ = 0;
};

}
