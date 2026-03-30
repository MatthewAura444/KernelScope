/*
 * otel_export.cpp — OpenTelemetry-compatible summary export for KernelScope.
 *
 * Emits a JSON summary following OTel semantic conventions.  Each
 * process appears as a "service", and latency data maps to span-like
 * attributes.  This is NOT a full OTLP exporter; it produces a JSON
 * document that downstream tools can ingest.
 */

#include "export/otel_export.hpp"

#include "core/event.hpp"
#include "util/time.hpp"

#include <nlohmann/json.hpp>

namespace kscope {

std::string export_otel(const Aggregator& agg) {
    nlohmann::json j;
    j["schema_url"] = "https://opentelemetry.io/schemas/1.21.0";
    j["resource_spans"] = nlohmann::json::array();

    for (const auto& [pid, ps] : agg.process_map()) {
        nlohmann::json span;
        span["resource"]["attributes"] = {
            {{"key", "service.name"}, {"value", {{"stringValue", ps.comm}}}},
            {{"key", "service.instance.id"}, {"value", {{"intValue", ps.pid}}}},
        };

        auto& scope_spans = span["scopeSpans"] = nlohmann::json::array();
        nlohmann::json scope;
        scope["scope"]["name"] = "kscope";
        scope["scope"]["version"] = "1.0.0";

        auto& spans = scope["spans"] = nlohmann::json::array();
        spans.push_back({
            {"name", "process.latency_summary"},
            {"kind", "SPAN_KIND_INTERNAL"},
            {"attributes", {
                {{"key", "kscope.total_syscall_ns"}, {"value", {{"intValue", ps.total_syscall_ns}}}},
                {{"key", "kscope.total_offcpu_ns"}, {"value", {{"intValue", ps.total_offcpu_ns}}}},
                {{"key", "kscope.max_latency_ns"}, {"value", {{"intValue", ps.max_latency_ns}}}},
                {{"key", "kscope.event_count"}, {"value", {{"intValue", ps.slow_event_count}}}},
                {{"key", "kscope.p95_ns"}, {"value", {{"intValue", ps.latency_hist.percentile(95)}}}},
                {{"key", "kscope.p99_ns"}, {"value", {{"intValue", ps.latency_hist.percentile(99)}}}},
            }},
        });

        scope_spans.push_back(scope);
        j["resource_spans"].push_back(span);
    }

    return j.dump(2);
}

}
