/*
 * commands.hpp — Command dispatch and execution for KernelScope.
 *
 * Each CLI subcommand (top, trace, record, replay, export, doctor)
 * has a corresponding run_* function that orchestrates the BPF loader,
 * collector, aggregator, and output formatting.
 */

#pragma once

#include "cli/parse_args.hpp"
#include "util/errors.hpp"

namespace kscope {

int run_command(const Config& cfg);

int run_top(const Config& cfg);
int run_top_offcpu(const Config& cfg);
int run_top_syscalls(const Config& cfg);
int run_trace(const Config& cfg);
int run_hot_stacks(const Config& cfg);
int run_record(const Config& cfg);
int run_replay(const Config& cfg);
int run_export(const Config& cfg);
int run_doctor(const Config& cfg);

}
