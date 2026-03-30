/*
 * screens.cpp — TUI screen implementations for KernelScope.
 *
 * Builds FTXUI Element trees for each of the six screens from live
 * aggregator data.  Each render function takes const references and
 * returns a single composable Element.
 */

#include "tui/screens.hpp"
#include "tui/tables.hpp"

#include "core/event.hpp"
#include "symbolization/stack_formatter.hpp"
#include "util/time.hpp"

namespace kscope {

using namespace ftxui;

Element render_overview(const Aggregator& agg, const Collector& coll, const Config& cfg) {
    auto procs = agg.top_processes(5, SortBy::TotalLatency);
    auto scs   = agg.top_syscalls(5);

    auto lost_style = coll.events_lost() > 0
        ? color(Color::Red) | bold
        : color(Color::Green);

    Elements rows;
    rows.push_back(hbox({
        text("Events: ") | dim,
        text(std::to_string(agg.total_events())) | bold,
        text("  received: ") | dim,
        text(std::to_string(coll.events_received())),
        text("  lost: ") | dim,
        text(std::to_string(coll.events_lost())) | lost_style,
    }));
    rows.push_back(separator());
    rows.push_back(text(" Top Processes") | bold | color(Color::Cyan));
    rows.push_back(render_process_table(procs));
    rows.push_back(separator());
    rows.push_back(text(" Top Syscalls") | bold | color(Color::Cyan));
    rows.push_back(render_syscall_table(scs));

    return vbox(std::move(rows));
}

Element render_processes(const Aggregator& agg, const Config& cfg) {
    auto procs = agg.top_processes(cfg.top_n, SortBy::TotalLatency);
    return vbox({
        text(" Processes — sorted by total latency") | bold | color(Color::Cyan),
        separator(),
        render_process_table(procs),
    });
}

Element render_threads(const Aggregator& agg, const Config& cfg) {
    auto threads = agg.top_threads(cfg.top_n, SortBy::TotalLatency);

    Elements header = {
        text("PID") | size(WIDTH, EQUAL, 8),
        text("TID") | size(WIDTH, EQUAL, 8),
        text("COMM") | size(WIDTH, EQUAL, 16),
        text("SYSCALL") | size(WIDTH, EQUAL, 14),
        text("OFFCPU") | size(WIDTH, EQUAL, 14),
        text("MAX") | size(WIDTH, EQUAL, 14),
        text("EVENTS") | size(WIDTH, EQUAL, 8),
    };

    Elements rows;
    rows.push_back(hbox(header) | bold | color(Color::Cyan));
    rows.push_back(separator());

    for (auto& t : threads) {
        rows.push_back(hbox({
            text(std::to_string(t.pid)) | size(WIDTH, EQUAL, 8),
            text(std::to_string(t.tid)) | size(WIDTH, EQUAL, 8),
            text(t.comm) | size(WIDTH, EQUAL, 16) | bold,
            text(time::format_duration_ns(t.total_syscall_ns)) | size(WIDTH, EQUAL, 14),
            text(time::format_duration_ns(t.total_offcpu_ns)) | size(WIDTH, EQUAL, 14),
            text(time::format_duration_ns(t.max_latency_ns)) | size(WIDTH, EQUAL, 14),
            text(std::to_string(t.slow_event_count)) | size(WIDTH, EQUAL, 8),
        }));
    }

    if (threads.empty())
        rows.push_back(text("  (no data yet)") | dim);

    return vbox({
        text(" Threads — sorted by total latency") | bold | color(Color::Cyan),
        separator(),
        vbox(std::move(rows)),
    });
}

Element render_syscalls(const Aggregator& agg, const Config& cfg) {
    auto scs = agg.top_syscalls(cfg.top_n);
    return vbox({
        text(" Syscalls — sorted by total time") | bold | color(Color::Cyan),
        separator(),
        render_syscall_table(scs),
    });
}

Element render_hot_stacks(const Aggregator& agg, SkeletonBridge& bridge,
                           Symbolizer& sym, const Config& cfg) {
    auto stacks = agg.top_stacks(cfg.top_n);

    Elements rows;
    if (stacks.empty()) {
        rows.push_back(text("  (no stack data — try --user-stacks or --kernel-stacks)") | dim);
    }

    for (size_t i = 0; i < stacks.size(); i++) {
        auto& st = stacks[i];
        rows.push_back(hbox({
            text("#" + std::to_string(i + 1)) | bold | color(Color::Cyan),
            text("  stack=" + std::to_string(st.stack_id)),
            text("  total="),
            text(std::string(time::format_duration_ns(st.total_ns))) | bold,
            text("  count=" + std::to_string(st.count)),
            text("  type="),
            text(std::string(event_type_name(st.representative_type))) | color(Color::Yellow),
        }));

        auto addrs = bridge.read_stack(static_cast<int32_t>(st.stack_id));
        auto frames = sym.resolve_stack(0, addrs, st.is_kernel);
        for (size_t f = 0; f < frames.size() && f < 8; f++) {
            auto style = frames[f].resolved
                ? color(Color::White)
                : color(Color::GrayDark);
            rows.push_back(text("    " + format_frame(frames[f], f)) | style);
        }
        if (frames.size() > 8)
            rows.push_back(text("    ... +" + std::to_string(frames.size() - 8) + " more") | dim);
        rows.push_back(separator());
    }

    return vbox({
        text(" Hot Stacks — sorted by attributed time") | bold | color(Color::Cyan),
        separator(),
        vbox(std::move(rows)),
    });
}

Element render_diagnostics(const Aggregator& agg, const Collector& coll,
                            const SkeletonBridge& bridge) {
    auto lost_style = coll.events_lost() > 0
        ? color(Color::Red) | bold
        : color(Color::Green);

    auto make_row = [](const std::string& label, const std::string& value, Decorator style = nothing) {
        return hbox({
            text("  " + label) | size(WIDTH, EQUAL, 24) | dim,
            text(value) | style,
        });
    };

    return vbox({
        text(" Diagnostics") | bold | color(Color::Cyan),
        separator(),
        text(" Pipeline") | bold,
        make_row("Total events:", std::to_string(agg.total_events()), bold),
        make_row("Events received:", std::to_string(coll.events_received())),
        make_row("Events lost:", std::to_string(coll.events_lost()), lost_style),
        separator(),
        text(" BPF Maps") | bold,
        make_row("Active syscalls:", std::to_string(bridge.active_syscall_count())),
        make_row("Sched state entries:", std::to_string(bridge.sched_state_count())),
    });
}

}
