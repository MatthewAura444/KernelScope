/*
 * collector.cpp — Ring buffer polling loop for KernelScope.
 */

#include "bpf/collector.hpp"

#include <bpf/libbpf.h>
#include <cstring>
#include <span>

#include "core/event_decoder.hpp"
#include "util/logging.hpp"

namespace kscope {

Collector::Collector(BpfLoader& loader) : loader_(loader) {}

Collector::~Collector() {
    stop();
    if (rb_)
        ring_buffer__free(rb_);
}

void Collector::set_callback(EventCallback cb) {
    callback_ = std::move(cb);
}

Status Collector::start() {
    int fd = loader_.ringbuf_fd();
    if (fd < 0)
        return std::unexpected(
            make_error(ErrorCode::BpfMapError, "invalid ring buffer fd"));

    rb_ = ring_buffer__new(fd, handle_event, this, nullptr);
    if (!rb_)
        return std::unexpected(
            make_error(ErrorCode::BpfMapError, "ring_buffer__new() failed"));

    running_.store(true, std::memory_order_release);
    log::info("Collector started, polling ring buffer");
    return {};
}

void Collector::stop() {
    running_.store(false, std::memory_order_release);
}

Status Collector::poll(int timeout_ms) {
    if (!rb_ || !running_.load(std::memory_order_acquire))
        return {};

    int err = ring_buffer__poll(rb_, timeout_ms);
    if (err < 0 && err != -EINTR)
        return std::unexpected(
            make_error(ErrorCode::BpfMapError,
                       std::format("ring_buffer__poll error: {}", err)));
    return {};
}

int Collector::handle_event(void* ctx, void* data, size_t size) {
    auto* self = static_cast<Collector*>(ctx);
    auto raw = std::span<const std::byte>(
        static_cast<const std::byte*>(data), size);

    auto event = decode_event(raw);
    if (!event) {
        self->decode_errors_.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    self->received_.fetch_add(1, std::memory_order_relaxed);

    if (self->callback_)
        self->callback_(*event);

    return 0;
}

uint64_t Collector::events_received() const {
    return received_.load(std::memory_order_relaxed);
}

uint64_t Collector::events_lost() const {
    return decode_errors_.load(std::memory_order_relaxed);
}

}
