/*
 * commands.cpp — Full command implementations for KernelScope.
 *
 * Each CLI subcommand is implemented here:  top, top-offcpu,
 * top-syscalls, trace, hot-stacks, record, replay, export, doctor.
 * Live commands share a common BPF setup helper and polling loop
 * pattern; display helpers format aggregated data for terminal output.
 */

#include "cli/commands.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <sys/utsname.h>

#include "bpf/collector.hpp"
#include "bpf/loader.hpp"
#include "bpf/skeleton_bridge.hpp"
#include "core/aggregator.hpp"
#include "core/classifications.hpp"
#include "core/event.hpp"
#include "export/json_export.hpp"
#include "export/otel_export.hpp"
#include "export/prometheus_export.hpp"
#include "kscope/api.hpp"
#include "record/recorder.hpp"
#include "record/replay.hpp"
#include "symbolization/stack_formatter.hpp"
#include "symbolization/symbolizer.hpp"
#include "util/logging.hpp"
#include "util/signal.hpp"
#include "util/time.hpp"

namespace kscope {

int run_command(const Config& cfg) {
    switch (cfg.command) {
        case Command::Top:         return run_top(cfg);
        case Command::TopOffcpu:   return run_top_offcpu(cfg);
        case Command::TopSyscalls: return run_top_syscalls(cfg);
        case Command::Trace:       return run_trace(cfg);
        case Command::HotStacks:   return run_hot_stacks(cfg);
        case Command::Record:      return run_record(cfg);
        case Command::Replay:      return run_replay(cfg);
        case Command::Export:       return run_export(cfg);
        case Command::Doctor:      return run_doctor(cfg);
        default:
            log::error("Unknown command");
            return 1;
    }
}

static BpfConfig make_bpf_config(const Config& cfg) {
    BpfConfig bc;
    bc.target_pid          = cfg.pid;
    bc.min_latency_ns      = time::us_to_ns(cfg.min_latency_us);
    bc.min_offcpu_ns       = time::us_to_ns(cfg.min_offcpu_us);
    bc.capture_user_stacks = cfg.user_stacks;
    bc.capture_kern_stacks = cfg.kernel_stacks;
    bc.stack_sampling_pct  = cfg.stack_sampling_pct;
    bc.target_comm         = cfg.comm;
    return bc;
}

static Status setup_bpf(BpfLoader& loader, const Config& cfg) {
    auto s = loader.open();
    if (!s) return s;
    s = loader.configure(make_bpf_config(cfg));
    if (!s) return s;
    s = loader.load();
    if (!s) return s;
    return loader.attach();
}

static bool expired(uint64_t start_ns, uint32_t duration_sec) {
    if (duration_sec == 0) return false;
    return (time::monotonic_ns() - start_ns) >= time::sec_to_ns(duration_sec);
}

static void clear_screen() {
    std::printf("\033[2J\033[H");
    std::fflush(stdout);
}

static void print_trace_event(const Event& ev) {
    std::printf("[%s] pid=%-6u tid=%-6u %-16.*s %-12s dur=%-12s class=%-16s",
                time::format_timestamp_ns(ev.timestamp_ns).c_str(),
                ev.pid, ev.tid,
                static_cast<int>(ev.comm_str().size()), ev.comm_str().data(),
                std::string(syscall_name(ev.syscall_id)).c_str(),
                time::format_duration_ns(ev.duration_ns).c_str(),
                std::string(latency_class_name(ev.classification)).c_str());
    if (ev.has_user_stack())
        std::printf(" ustack=%ld", ev.user_stack_id);
    if (ev.has_kern_stack())
        std::printf(" kstack=%ld", ev.kern_stack_id);
    std::printf("\n");
}

int run_trace(const Config& cfg) {
    BpfLoader loader;
    if (auto s = setup_bpf(loader, cfg); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Collector collector(loader);
    collector.set_callback([](const Event& ev) {
        Event c = ev;
        c.classification = classify_event(ev);
        print_trace_event(c);
    });

    if (auto s = collector.start(); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    auto t0 = time::monotonic_ns();
    while (!shutdown_requested()) {
        collector.poll(100);
        if (expired(t0, cfg.duration_sec)) break;
    }

    log::info("Trace done: {} received, {} lost",
              collector.events_received(), collector.events_lost());
    return 0;
}

static void print_process_table(const char* title,
                                const std::vector<ProcessStats>& procs,
                                uint64_t recv, uint64_t lost) {
    clear_screen();
    std::printf("\033[1mKernelScope — %s\033[0m\n\n", title);
    std::printf("  %-8s %-16s %12s %12s %12s %8s %12s\n",
                "PID", "COMM", "SYSCALL", "OFFCPU", "MAX", "EVENTS", "P95");
    std::printf("  %-8s %-16s %12s %12s %12s %8s %12s\n",
                "───", "────", "───────", "──────", "───", "──────", "───");
    for (auto& p : procs) {
        std::printf("  %-8u %-16.16s %12s %12s %12s %8u %12s\n",
                    p.pid,
                    p.comm.c_str(),
                    time::format_duration_ns(p.total_syscall_ns).c_str(),
                    time::format_duration_ns(p.total_offcpu_ns).c_str(),
                    time::format_duration_ns(p.max_latency_ns).c_str(),
                    p.slow_event_count,
                    time::format_duration_ns(p.latency_hist.percentile(95)).c_str());
    }
    std::printf("\n  [events: %lu received, %lu lost]\n", recv, lost);
    std::fflush(stdout);
}

int run_top(const Config& cfg) {
    BpfLoader loader;
    if (auto s = setup_bpf(loader, cfg); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Aggregator agg;
    Collector collector(loader);
    collector.set_callback([&](const Event& ev) {
        Event c = ev;
        c.classification = classify_event(ev);
        agg.process_event(c);
    });

    if (auto s = collector.start(); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    auto t0 = time::monotonic_ns();
    auto last_draw = t0;
    uint64_t window_ns = time::sec_to_ns(cfg.window_sec);

    while (!shutdown_requested()) {
        collector.poll(100);
        auto now = time::monotonic_ns();

        if (now - last_draw >= 1'000'000'000) {
            auto procs = agg.top_processes(cfg.top_n, SortBy::TotalLatency);
            print_process_table("Top Latency Processes",
                                procs,
                                collector.events_received(),
                                collector.events_lost());
            last_draw = now;
        }

        if (window_ns > 0 && (now - t0) >= window_ns) {
            agg.reset();
            t0 = now;
        }
        if (expired(t0, cfg.duration_sec)) break;
    }

    if (cfg.json_output)
        std::printf("%s\n", export_json(agg).c_str());

    return 0;
}

int run_top_offcpu(const Config& cfg) {
    BpfLoader loader;
    if (auto s = setup_bpf(loader, cfg); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Aggregator agg;
    Collector collector(loader);
    collector.set_callback([&](const Event& ev) {
        Event c = ev;
        c.classification = classify_event(ev);
        agg.process_event(c);
    });

    if (auto s = collector.start(); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    auto t0 = time::monotonic_ns();
    auto last_draw = t0;

    while (!shutdown_requested()) {
        collector.poll(100);
        auto now = time::monotonic_ns();

        if (now - last_draw >= 1'000'000'000) {
            auto procs = agg.top_processes(cfg.top_n, SortBy::OffcpuTime);
            print_process_table("Top Off-CPU Processes",
                                procs,
                                collector.events_received(),
                                collector.events_lost());
            last_draw = now;
        }

        if (time::sec_to_ns(cfg.window_sec) > 0 &&
            (now - t0) >= time::sec_to_ns(cfg.window_sec)) {
            agg.reset();
            t0 = now;
        }
        if (expired(t0, cfg.duration_sec)) break;
    }
    return 0;
}

static void print_syscall_table(const std::vector<SyscallStats>& scs,
                                uint64_t recv, uint64_t lost) {
    clear_screen();
    std::printf("\033[1mKernelScope — Top Syscalls by Latency\033[0m\n\n");
    std::printf("  %-14s %10s %12s %12s %12s %12s %6s\n",
                "SYSCALL", "COUNT", "TOTAL", "AVG", "P95", "MAX", "PIDS");
    std::printf("  %-14s %10s %12s %12s %12s %12s %6s\n",
                "───────", "─────", "─────", "───", "───", "───", "────");
    for (auto& s : scs) {
        auto avg = s.count > 0 ? s.total_ns / s.count : 0UL;
        std::printf("  %-14s %10lu %12s %12s %12s %12s %6zu\n",
                    std::string(syscall_name(s.syscall_id)).c_str(),
                    s.count,
                    time::format_duration_ns(s.total_ns).c_str(),
                    time::format_duration_ns(avg).c_str(),
                    time::format_duration_ns(s.latency_hist.percentile(95)).c_str(),
                    time::format_duration_ns(s.max_ns).c_str(),
                    s.affected_pids.size());
    }
    std::printf("\n  [events: %lu received, %lu lost]\n", recv, lost);
    std::fflush(stdout);
}

int run_top_syscalls(const Config& cfg) {
    BpfLoader loader;
    if (auto s = setup_bpf(loader, cfg); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Aggregator agg;
    Collector collector(loader);
    collector.set_callback([&](const Event& ev) {
        Event c = ev;
        c.classification = classify_event(ev);
        agg.process_event(c);
    });

    if (auto s = collector.start(); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    auto t0 = time::monotonic_ns();
    auto last_draw = t0;

    while (!shutdown_requested()) {
        collector.poll(100);
        auto now = time::monotonic_ns();

        if (now - last_draw >= 1'000'000'000) {
            auto scs = agg.top_syscalls(cfg.top_n);
            print_syscall_table(scs,
                                collector.events_received(),
                                collector.events_lost());
            last_draw = now;
        }

        if (time::sec_to_ns(cfg.window_sec) > 0 &&
            (now - t0) >= time::sec_to_ns(cfg.window_sec)) {
            agg.reset();
            t0 = now;
        }
        if (expired(t0, cfg.duration_sec)) break;
    }
    return 0;
}

int run_hot_stacks(const Config& cfg) {
    BpfLoader loader;
    if (auto s = setup_bpf(loader, cfg); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    SkeletonBridge bridge(loader);
    Symbolizer sym;
    Aggregator agg;
    Collector collector(loader);

    collector.set_callback([&](const Event& ev) {
        Event c = ev;
        c.classification = classify_event(ev);
        agg.process_event(c);
    });

    if (auto s = collector.start(); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    auto t0 = time::monotonic_ns();
    auto last_draw = t0;

    while (!shutdown_requested()) {
        collector.poll(100);
        auto now = time::monotonic_ns();

        if (now - last_draw >= 1'000'000'000) {
            clear_screen();
            std::printf("\033[1mKernelScope — Hot Stacks\033[0m\n\n");
            auto stacks = agg.top_stacks(cfg.top_n);

            for (size_t i = 0; i < stacks.size(); i++) {
                auto& st = stacks[i];
                std::printf("  #%zu  stack_id=%ld  pid=%u  total=%s  count=%lu  type=%s%s\n",
                            i + 1, st.stack_id, st.pid,
                            time::format_duration_ns(st.total_ns).c_str(),
                            st.count,
                            std::string(event_type_name(st.representative_type)).c_str(),
                            st.is_kernel ? " [kernel]" : "");

                auto addrs = bridge.read_stack(static_cast<int32_t>(st.stack_id));
                auto frames = sym.resolve_stack(st.pid, addrs, st.is_kernel);
                for (size_t f = 0; f < frames.size() && f < 8; f++)
                    std::printf("      %s\n", format_frame(frames[f], f).c_str());
                std::printf("\n");
            }

            std::printf("  [events: %lu received, %lu lost]\n",
                        collector.events_received(), collector.events_lost());
            std::fflush(stdout);
            last_draw = now;
        }

        if (time::sec_to_ns(cfg.window_sec) > 0 &&
            (now - t0) >= time::sec_to_ns(cfg.window_sec)) {
            agg.reset();
            t0 = now;
        }
        if (expired(t0, cfg.duration_sec)) break;
    }
    return 0;
}

int run_record(const Config& cfg) {
    BpfLoader loader;
    if (auto s = setup_bpf(loader, cfg); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Recorder rec;
    if (auto s = rec.open(cfg.record_file); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Collector collector(loader);
    collector.set_callback([&](const Event& ev) {
        Event c = ev;
        c.classification = classify_event(ev);
        rec.write_event(c);
    });

    if (auto s = collector.start(); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    auto t0 = time::monotonic_ns();
    while (!shutdown_requested()) {
        collector.poll(100);
        if (expired(t0, cfg.duration_sec)) break;
    }

    rec.close();
    std::printf("Recorded %lu events to %s\n",
                rec.events_written(), cfg.record_file.c_str());
    return 0;
}

int run_replay(const Config& cfg) {
    ReplaySource src;
    if (auto s = src.open(cfg.replay_file); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Aggregator agg;
    auto s = src.replay([&](const Event& ev) {
        agg.process_event(ev);
    });
    if (!s) {
        log::error("{}", s.error().message);
        return 1;
    }
    src.close();

    if (cfg.json_output) {
        std::printf("%s\n", export_json(agg).c_str());
    } else {
        auto procs = agg.top_processes(cfg.top_n, SortBy::TotalLatency);
        std::printf("KernelScope — Replay: %s (%lu events)\n\n",
                    cfg.replay_file.c_str(), src.events_replayed());
        std::printf("  %-8s %-16s %12s %12s %12s %8s %12s\n",
                    "PID", "COMM", "SYSCALL", "OFFCPU", "MAX", "EVENTS", "P95");
        std::printf("  %-8s %-16s %12s %12s %12s %8s %12s\n",
                    "───", "────", "───────", "──────", "───", "──────", "───");
        for (auto& p : procs) {
            std::printf("  %-8u %-16.16s %12s %12s %12s %8u %12s\n",
                        p.pid, p.comm.c_str(),
                        time::format_duration_ns(p.total_syscall_ns).c_str(),
                        time::format_duration_ns(p.total_offcpu_ns).c_str(),
                        time::format_duration_ns(p.max_latency_ns).c_str(),
                        p.slow_event_count,
                        time::format_duration_ns(p.latency_hist.percentile(95)).c_str());
        }

        std::printf("\n  Syscalls:\n");
        auto scs = agg.top_syscalls(cfg.top_n);
        for (auto& sc : scs) {
            auto avg = sc.count > 0 ? sc.total_ns / sc.count : 0UL;
            std::printf("    %-14s count=%-8lu total=%-12s avg=%-12s max=%s\n",
                        std::string(syscall_name(sc.syscall_id)).c_str(),
                        sc.count,
                        time::format_duration_ns(sc.total_ns).c_str(),
                        time::format_duration_ns(avg).c_str(),
                        time::format_duration_ns(sc.max_ns).c_str());
        }
    }
    return 0;
}

int run_export(const Config& cfg) {
    BpfLoader loader;
    if (auto s = setup_bpf(loader, cfg); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    Aggregator agg;
    Collector collector(loader);
    collector.set_callback([&](const Event& ev) {
        Event c = ev;
        c.classification = classify_event(ev);
        agg.process_event(c);
    });

    if (auto s = collector.start(); !s) {
        log::error("{}", s.error().message);
        return 1;
    }

    uint32_t dur = cfg.duration_sec > 0 ? cfg.duration_sec : cfg.window_sec;
    auto t0 = time::monotonic_ns();
    while (!shutdown_requested()) {
        collector.poll(100);
        if (expired(t0, dur)) break;
    }

    std::string output;
    if (cfg.export_format == "otel") {
        output = export_otel(agg);
    } else if (cfg.export_format == "prometheus") {
        output = export_prometheus(agg);
    } else {
        output = export_json(agg);
    }
    if (cfg.export_file.empty()) {
        std::printf("%s\n", output.c_str());
    } else {
        std::ofstream f(cfg.export_file);
        f << output;
        log::info("Exported to {}", cfg.export_file);
    }
    return 0;
}

static bool check_kernel_version() {
    struct utsname info;
    if (uname(&info) != 0) {
        std::printf("  kernel version:  UNKNOWN (uname failed)\n");
        return false;
    }
    unsigned major = 0, minor = 0;
    std::sscanf(info.release, "%u.%u", &major, &minor);
    bool ok = (major > kMinKernelMajor) ||
              (major == kMinKernelMajor && minor >= kMinKernelMinor);
    std::printf("  kernel version:  %s %s\n", info.release,
                ok ? "✓" : "✗ (need ≥ 6.1)");
    return ok;
}

static bool check_btf() {
    bool ok = std::filesystem::exists("/sys/kernel/btf/vmlinux");
    std::printf("  BTF available:   %s\n", ok ? "yes ✓" : "no ✗");
    return ok;
}

static bool check_bpf_access() {
    bool root = (geteuid() == 0);
    std::printf("  running as root: %s\n",
                root ? "yes ✓" : "no (may need CAP_BPF)");
    return root;
}

static bool check_tracefs() {
    bool ok = std::filesystem::exists("/sys/kernel/debug/tracing") ||
              std::filesystem::exists("/sys/kernel/tracing");
    std::printf("  tracefs:         %s\n", ok ? "available ✓" : "not found ✗");
    return ok;
}

int run_doctor(const Config&) {
    std::printf("KernelScope v%.*s — System Compatibility Check\n\n",
                static_cast<int>(kVersion.size()), kVersion.data());

    int issues = 0;
    if (!check_kernel_version()) issues++;
    if (!check_btf())            issues++;
    if (!check_bpf_access())     issues++;
    if (!check_tracefs())        issues++;

    struct utsname info;
    if (uname(&info) == 0) {
        bool x86 = (std::strcmp(info.machine, "x86_64") == 0);
        bool arm = (std::strcmp(info.machine, "aarch64") == 0);
        std::printf("  architecture:    %s %s\n", info.machine,
                    (x86 || arm) ? "✓" : "(untested)");
    }

    std::printf("\n");
    if (issues == 0) {
        std::printf("All checks passed. Ready to profile.\n");
        return 0;
    }
    std::printf("%d issue(s) found. Some features may not work.\n", issues);
    return 1;
}

}
