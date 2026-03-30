/*
 * parse_args.cpp — CLI argument parsing implementation for KernelScope.
 *
 * Uses CLI11 for subcommand-based argument parsing.  Maps the full
 * set of flags from the spec (--pid, --comm, --duration, thresholds,
 * stack flags, record/replay, export, etc.) into the Config struct.
 */

#include "cli/parse_args.hpp"

#include <CLI/CLI.hpp>
#include <cstdlib>

#include "kscope/api.hpp"

namespace kscope {

Config parse_args(int argc, char* argv[]) {
    Config cfg;

    CLI::App app{
        std::string(kName) + " v" + std::string(kVersion) + " — eBPF latency profiler",
        std::string(kBinaryName)};

    app.require_subcommand(1);

    auto add_common = [&](CLI::App* sub) {
        sub->add_option("--pid", cfg.pid, "Filter by PID");
        sub->add_option("--comm", cfg.comm, "Filter by process name");
        sub->add_option("--duration", cfg.duration_sec, "Run duration in seconds (0=infinite)");
        sub->add_option("--min-latency-us", cfg.min_latency_us, "Minimum syscall latency threshold (µs)");
        sub->add_option("--min-offcpu-us", cfg.min_offcpu_us, "Minimum off-CPU threshold (µs)");
        sub->add_option("--window", cfg.window_sec, "Aggregation window in seconds");
        sub->add_option("--top", cfg.top_n, "Number of top entries to show");
        sub->add_option("--stack-sampling", cfg.stack_sampling_pct, "Stack sampling percentage (0-100)");
        sub->add_flag("--user-stacks,!--no-user-stacks", cfg.user_stacks, "Capture user-space stacks");
        sub->add_flag("--kernel-stacks,!--no-kernel-stacks", cfg.kernel_stacks, "Capture kernel stacks");
        sub->add_flag("--json", cfg.json_output, "JSON output");
        sub->add_flag("--verbose,-v", cfg.verbose, "Verbose output");
        sub->add_flag("--quiet,-q", cfg.quiet, "Quiet output");
        sub->add_option("--config,-c", cfg.config_file, "Config file path");
    };

    auto* top_cmd = app.add_subcommand("top", "Show top latency processes/threads");
    add_common(top_cmd);
    top_cmd->callback([&] { cfg.command = Command::Top; });

    auto* top_offcpu = app.add_subcommand("top-offcpu", "Show top off-CPU processes");
    add_common(top_offcpu);
    top_offcpu->callback([&] { cfg.command = Command::TopOffcpu; });

    auto* top_syscalls = app.add_subcommand("top-syscalls", "Show top syscalls by latency");
    add_common(top_syscalls);
    top_syscalls->callback([&] { cfg.command = Command::TopSyscalls; });

    auto* trace_cmd = app.add_subcommand("trace", "Trace events for a process");
    add_common(trace_cmd);
    trace_cmd->callback([&] { cfg.command = Command::Trace; });

    auto* hot_stacks = app.add_subcommand("hot-stacks", "Show hottest stack traces");
    add_common(hot_stacks);
    hot_stacks->callback([&] { cfg.command = Command::HotStacks; });

    auto* record_cmd = app.add_subcommand("record", "Record events to a .ksr file");
    add_common(record_cmd);
    record_cmd->add_option("--output,-o", cfg.record_file, "Output file path")->required();
    record_cmd->callback([&] { cfg.command = Command::Record; });

    auto* replay_cmd = app.add_subcommand("replay", "Replay a recorded .ksr file");
    replay_cmd->add_option("file", cfg.replay_file, "KSR file to replay")->required();
    replay_cmd->add_option("--top", cfg.top_n, "Number of top entries");
    replay_cmd->add_flag("--json", cfg.json_output, "JSON output");
    replay_cmd->add_flag("--verbose,-v", cfg.verbose, "Verbose output");
    replay_cmd->callback([&] { cfg.command = Command::Replay; });

    auto* export_cmd = app.add_subcommand("export", "Export current data");
    add_common(export_cmd);
    export_cmd->add_option("--output,-o", cfg.export_file, "Output file (stdout if omitted)");
    export_cmd->add_option("--format,-f", cfg.export_format,
                           "Export format: json, otel, prometheus (default: json)");
    export_cmd->callback([&] { cfg.command = Command::Export; });

    auto* doctor_cmd = app.add_subcommand("doctor", "Check system compatibility");
    doctor_cmd->add_flag("--verbose,-v", cfg.verbose, "Verbose diagnostics");
    doctor_cmd->callback([&] { cfg.command = Command::Doctor; });

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
        std::exit(e.get_exit_code());
    }

    return cfg;
}

}
