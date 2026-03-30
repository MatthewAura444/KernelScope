/*
 * app.hpp — TUI application for KernelScope.
 *
 * FTXUI-based terminal UI with 6 screens: Overview, Processes,
 * Threads, Syscalls, Hot Stacks, Diagnostics.  Drives a Collector
 * and Aggregator, refreshing at a configurable interval.
 */

#pragma once

#include "bpf/collector.hpp"
#include "bpf/loader.hpp"
#include "bpf/skeleton_bridge.hpp"
#include "cli/parse_args.hpp"
#include "core/aggregator.hpp"
#include "symbolization/symbolizer.hpp"

namespace kscope {

class TuiApp {
public:
    explicit TuiApp(const Config& cfg);
    int run();

private:
    Config cfg_;
};

}
