/*
 * json_export.hpp — JSON snapshot export for KernelScope.
 *
 * Exports the current aggregation window as a JSON document with
 * per-process, per-syscall, per-stack statistics and diagnostics.
 */

#pragma once

#include "core/aggregator.hpp"

#include <ostream>
#include <string>

namespace kscope {

std::string export_json(const Aggregator& agg);
void export_json(const Aggregator& agg, std::ostream& out);

}
