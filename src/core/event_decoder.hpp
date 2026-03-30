/*
 * event_decoder.hpp — Decodes raw ring buffer bytes into typed Event objects.
 *
 * The decoder is the first user-space stage: it takes the raw struct event
 * from BPF and produces a kscope::Event that the rest of the pipeline
 * (classifier, aggregator, exporter) consumes.
 */

#pragma once

#include "core/event.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace kscope {

std::optional<Event> decode_event(std::span<const std::byte> raw);

}
