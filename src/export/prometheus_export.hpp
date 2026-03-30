/*
 * prometheus_export.hpp — Prometheus text exposition export for KernelScope.
 *
 * Exports metrics in Prometheus text format: counters, gauges, and
 * histogram-style summaries per process, syscall, and event type.
 */

#pragma once

#include "core/aggregator.hpp"

#include <string>

namespace kscope {

std::string export_prometheus(const Aggregator& agg);

}
