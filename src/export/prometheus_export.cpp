/*
 * prometheus_export.cpp — Prometheus text exposition format for KernelScope.
 *
 * Emits metrics following the Prometheus exposition format (text/plain;
 * version=0.0.4).  Includes per-process event counts, latency summaries,
 * and per-syscall statistics.
 */

#include "export/prometheus_export.hpp"

#include "core/event.hpp"

#include <format>

namespace kscope {

static std::string escape_label(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\')      out += "\\\\";
        else if (c == '"')  out += "\\\"";
        else if (c == '\n') out += "\\n";
        else                out += c;
    }
    return out;
}

std::string export_prometheus(const Aggregator& agg) {
    std::string out;

    out += "# HELP kscope_events_total Total slow events per process.\n";
    out += "# TYPE kscope_events_total counter\n";
    for (const auto& [pid, ps] : agg.process_map()) {
        out += std::format("kscope_events_total{{pid=\"{}\",comm=\"{}\"}} {}\n",
                           ps.pid, escape_label(ps.comm), ps.slow_event_count);
    }

    out += "# HELP kscope_syscall_seconds_total Total syscall latency per process.\n";
    out += "# TYPE kscope_syscall_seconds_total counter\n";
    for (const auto& [pid, ps] : agg.process_map()) {
        out += std::format("kscope_syscall_seconds_total{{pid=\"{}\",comm=\"{}\"}} {:.9f}\n",
                           ps.pid, escape_label(ps.comm),
                           static_cast<double>(ps.total_syscall_ns) / 1e9);
    }

    out += "# HELP kscope_offcpu_seconds_total Total off-CPU time per process.\n";
    out += "# TYPE kscope_offcpu_seconds_total counter\n";
    for (const auto& [pid, ps] : agg.process_map()) {
        out += std::format("kscope_offcpu_seconds_total{{pid=\"{}\",comm=\"{}\"}} {:.9f}\n",
                           ps.pid, escape_label(ps.comm),
                           static_cast<double>(ps.total_offcpu_ns) / 1e9);
    }

    out += "# HELP kscope_max_latency_seconds Maximum single-event latency.\n";
    out += "# TYPE kscope_max_latency_seconds gauge\n";
    for (const auto& [pid, ps] : agg.process_map()) {
        out += std::format("kscope_max_latency_seconds{{pid=\"{}\",comm=\"{}\"}} {:.9f}\n",
                           ps.pid, escape_label(ps.comm),
                           static_cast<double>(ps.max_latency_ns) / 1e9);
    }

    out += "# HELP kscope_syscall_calls_total Syscall invocation count.\n";
    out += "# TYPE kscope_syscall_calls_total counter\n";
    for (const auto& [id, sc] : agg.syscall_map()) {
        out += std::format("kscope_syscall_calls_total{{syscall=\"{}\"}} {}\n",
                           std::string(syscall_name(sc.syscall_id)), sc.count);
    }

    out += "# HELP kscope_syscall_duration_seconds_total Total time in syscall.\n";
    out += "# TYPE kscope_syscall_duration_seconds_total counter\n";
    for (const auto& [id, sc] : agg.syscall_map()) {
        out += std::format("kscope_syscall_duration_seconds_total{{syscall=\"{}\"}} {:.9f}\n",
                           std::string(syscall_name(sc.syscall_id)),
                           static_cast<double>(sc.total_ns) / 1e9);
    }

    return out;
}

}
