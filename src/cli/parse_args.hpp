/*
 * parse_args.hpp — CLI argument parsing for KernelScope.
 *
 * Defines the Config structure holding all runtime configuration
 * (CLI flags + config file values) and the parse function that
 * populates it from argc/argv using CLI11.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kscope {

enum class Command {
    Top,
    TopOffcpu,
    TopSyscalls,
    Trace,
    HotStacks,
    Record,
    Replay,
    Export,
    Doctor,
    Help,
};

struct Config {
    Command command = Command::Top;

    uint32_t pid = 0;
    std::string comm;
    std::optional<uint64_t> syscall_filter;

    uint64_t min_latency_us = 100;
    uint64_t min_offcpu_us  = 1000;

    uint32_t duration_sec = 0;
    uint32_t window_sec   = 5;
    uint32_t top_n        = 20;

    uint32_t stack_sampling_pct = 100;
    bool user_stacks  = true;
    bool kernel_stacks = false;

    bool json_output = false;
    std::string export_format = "json";
    std::string record_file;
    std::string replay_file;
    std::string export_file;

    std::string config_file;

    bool verbose = false;
    bool quiet   = false;
};

Config parse_args(int argc, char* argv[]);

}
