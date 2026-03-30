/*
 * collector.hpp — Ring buffer polling and event dispatch for KernelScope.
 *
 * The collector is the bridge between the BPF ring buffer and the
 * user-space processing pipeline.  It polls for new events, decodes
 * them, and dispatches to registered callbacks (aggregator, recorder,
 * CLI output, etc.).
 */

#pragma once

#include "bpf/loader.hpp"
#include "core/event.hpp"
#include "util/errors.hpp"

#include <atomic>
#include <cstdint>
#include <functional>

struct ring_buffer;

namespace kscope {

using EventCallback = std::function<void(const Event&)>;

class Collector {
public:
    explicit Collector(BpfLoader& loader);
    ~Collector();

    Collector(const Collector&) = delete;
    Collector& operator=(const Collector&) = delete;

    void set_callback(EventCallback cb);

    Status start();
    void   stop();
    Status poll(int timeout_ms = 100);

    uint64_t events_received() const;
    uint64_t events_lost() const;

private:
    static int handle_event(void* ctx, void* data, size_t size);

    BpfLoader& loader_;
    struct ring_buffer* rb_ = nullptr;
    EventCallback callback_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> received_{0};
    std::atomic<uint64_t> decode_errors_{0};
};

}
