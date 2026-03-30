/*
 * json_export.cpp — JSON snapshot export for KernelScope.
 *
 * Exports the full aggregation state as a structured JSON document
 * with per-process, per-syscall statistics and diagnostics metadata.
 */

#include "export/json_export.hpp"

#include "core/event.hpp"
#include "util/time.hpp"

#include <nlohmann/json.hpp>

namespace kscope {

std::string export_json(const Aggregator& agg) {
    nlohmann::json j;
    j["version"] = "1.0.0";
    j["format"]  = "kscope-snapshot";

    auto& procs = j["processes"] = nlohmann::json::array();
    for (const auto& [pid, ps] : agg.process_map()) {
        procs.push_back({
            {"pid",              ps.pid},
            {"comm",             ps.comm},
            {"total_syscall_ns", ps.total_syscall_ns},
            {"total_offcpu_ns",  ps.total_offcpu_ns},
            {"max_latency_ns",   ps.max_latency_ns},
            {"slow_event_count", ps.slow_event_count},
            {"p50_ns",           ps.latency_hist.percentile(50)},
            {"p95_ns",           ps.latency_hist.percentile(95)},
            {"p99_ns",           ps.latency_hist.percentile(99)},
        });
    }

    auto& syscalls = j["syscalls"] = nlohmann::json::array();
    for (const auto& [id, sc] : agg.syscall_map()) {
        auto avg = sc.count > 0 ? sc.total_ns / sc.count : 0ULL;
        nlohmann::json pids = nlohmann::json::array();
        for (auto p : sc.affected_pids) pids.push_back(p);

        syscalls.push_back({
            {"syscall",       std::string(syscall_name(sc.syscall_id))},
            {"syscall_id",    sc.syscall_id},
            {"count",         sc.count},
            {"total_ns",      sc.total_ns},
            {"avg_ns",        avg},
            {"max_ns",        sc.max_ns},
            {"p95_ns",        sc.latency_hist.percentile(95)},
            {"affected_pids", pids},
        });
    }

    j["diagnostics"] = {
        {"total_events", agg.total_events()},
    };

    return j.dump(2);
}

void export_json(const Aggregator& agg, std::ostream& out) {
    out << export_json(agg);
}

}
