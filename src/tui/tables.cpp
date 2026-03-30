/*
 * tables.cpp — FTXUI table rendering for KernelScope.
 *
 * Builds styled tables from stats vectors with headers, separators,
 * and coloured highlights for high-latency entries.  Uses a traffic-
 * light palette: green < 100ms, yellow < 1s, red >= 1s, bright red >= 10s.
 */

#include "tui/tables.hpp"

#include "core/event.hpp"
#include "util/time.hpp"

namespace kscope {

using namespace ftxui;

static Decorator latency_color(uint64_t ns) {
    if (ns >= 10'000'000'000ULL) return color(Color::RedLight) | bold;
    if (ns >=  1'000'000'000ULL) return color(Color::Red);
    if (ns >=    100'000'000ULL) return color(Color::Yellow);
    if (ns >=     10'000'000ULL) return color(Color::White);
    return dim;
}

Element render_process_table(const std::vector<ProcessStats>& procs) {
    Elements header = {
        text("PID") | size(WIDTH, EQUAL, 8),
        text("COMM") | size(WIDTH, EQUAL, 16),
        text("SYSCALL") | size(WIDTH, EQUAL, 14),
        text("OFFCPU") | size(WIDTH, EQUAL, 14),
        text("MAX") | size(WIDTH, EQUAL, 14),
        text("EVENTS") | size(WIDTH, EQUAL, 8),
        text("P95") | size(WIDTH, EQUAL, 14),
    };

    Elements rows;
    rows.push_back(hbox(header) | bold | color(Color::Cyan));
    rows.push_back(separator());

    for (auto& p : procs) {
        rows.push_back(hbox({
            text(std::to_string(p.pid)) | size(WIDTH, EQUAL, 8),
            text(p.comm) | size(WIDTH, EQUAL, 16) | bold,
            text(time::format_duration_ns(p.total_syscall_ns)) | size(WIDTH, EQUAL, 14) |
                latency_color(p.total_syscall_ns),
            text(time::format_duration_ns(p.total_offcpu_ns)) | size(WIDTH, EQUAL, 14) |
                latency_color(p.total_offcpu_ns),
            text(time::format_duration_ns(p.max_latency_ns)) | size(WIDTH, EQUAL, 14) |
                latency_color(p.max_latency_ns),
            text(std::to_string(p.slow_event_count)) | size(WIDTH, EQUAL, 8),
            text(time::format_duration_ns(p.latency_hist.percentile(95))) | size(WIDTH, EQUAL, 14),
        }));
    }

    if (procs.empty())
        rows.push_back(text("  (no data yet)") | dim);

    return vbox(std::move(rows));
}

Element render_syscall_table(const std::vector<SyscallStats>& scs) {
    Elements header = {
        text("SYSCALL") | size(WIDTH, EQUAL, 14),
        text("COUNT") | size(WIDTH, EQUAL, 10),
        text("TOTAL") | size(WIDTH, EQUAL, 14),
        text("AVG") | size(WIDTH, EQUAL, 14),
        text("P95") | size(WIDTH, EQUAL, 14),
        text("MAX") | size(WIDTH, EQUAL, 14),
        text("PIDS") | size(WIDTH, EQUAL, 6),
    };

    Elements rows;
    rows.push_back(hbox(header) | bold | color(Color::Cyan));
    rows.push_back(separator());

    for (auto& s : scs) {
        auto avg = s.count > 0 ? s.total_ns / s.count : 0ULL;
        rows.push_back(hbox({
            text(std::string(syscall_name(s.syscall_id))) | size(WIDTH, EQUAL, 14) | bold,
            text(std::to_string(s.count)) | size(WIDTH, EQUAL, 10),
            text(time::format_duration_ns(s.total_ns)) | size(WIDTH, EQUAL, 14) |
                latency_color(s.total_ns),
            text(time::format_duration_ns(avg)) | size(WIDTH, EQUAL, 14),
            text(time::format_duration_ns(s.latency_hist.percentile(95))) | size(WIDTH, EQUAL, 14),
            text(time::format_duration_ns(s.max_ns)) | size(WIDTH, EQUAL, 14) |
                latency_color(s.max_ns),
            text(std::to_string(s.affected_pids.size())) | size(WIDTH, EQUAL, 6),
        }));
    }

    if (scs.empty())
        rows.push_back(text("  (no data yet)") | dim);

    return vbox(std::move(rows));
}

}
