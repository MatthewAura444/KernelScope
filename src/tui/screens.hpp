/*
 * screens.hpp — TUI screen rendering functions for KernelScope.
 *
 * Each function returns an FTXUI Element representing one of the
 * six main screens.  Called by TuiApp on each render cycle.
 */

#pragma once

#include <ftxui/dom/elements.hpp>

#include "bpf/collector.hpp"
#include "bpf/skeleton_bridge.hpp"
#include "cli/parse_args.hpp"
#include "core/aggregator.hpp"
#include "symbolization/symbolizer.hpp"

namespace kscope {

ftxui::Element render_overview(const Aggregator& agg, const Collector& coll, const Config& cfg);
ftxui::Element render_processes(const Aggregator& agg, const Config& cfg);
ftxui::Element render_threads(const Aggregator& agg, const Config& cfg);
ftxui::Element render_syscalls(const Aggregator& agg, const Config& cfg);
ftxui::Element render_hot_stacks(const Aggregator& agg, SkeletonBridge& bridge,
                                  Symbolizer& sym, const Config& cfg);
ftxui::Element render_diagnostics(const Aggregator& agg, const Collector& coll,
                                   const SkeletonBridge& bridge);

}
