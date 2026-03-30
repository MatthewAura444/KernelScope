/*
 * otel_export.hpp — OpenTelemetry-compatible export for KernelScope.
 *
 * Exports events as OTel-style spans/events summary following
 * OpenTelemetry semantic conventions.
 */

#pragma once

#include "core/aggregator.hpp"

#include <string>

namespace kscope {

std::string export_otel(const Aggregator& agg);

}
