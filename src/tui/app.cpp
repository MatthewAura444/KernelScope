/*
 * app.cpp — FTXUI-based terminal UI for KernelScope.
 *
 * Provides a full-screen terminal interface with tabbed navigation
 * across six screens.  Auto-refreshes every 500ms for live data.
 * The application owns the BPF pipeline and runs the collector
 * polling inside the render callback.
 */

#include "tui/app.hpp"
#include "tui/screens.hpp"
#include "tui/tables.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <thread>

#include "core/classifications.hpp"
#include "kscope/api.hpp"
#include "util/logging.hpp"
#include "util/signal.hpp"
#include "util/time.hpp"

namespace kscope {

TuiApp::TuiApp(const Config& cfg) : cfg_(cfg) {}

int TuiApp::run() {
    BpfLoader loader;
    {
        BpfConfig bc;
        bc.target_pid          = cfg_.pid;
        bc.min_latency_ns      = time::us_to_ns(cfg_.min_latency_us);
        bc.min_offcpu_ns       = time::us_to_ns(cfg_.min_offcpu_us);
        bc.capture_user_stacks = cfg_.user_stacks;
        bc.capture_kern_stacks = cfg_.kernel_stacks;
        bc.stack_sampling_pct  = cfg_.stack_sampling_pct;
        bc.target_comm         = cfg_.comm;

        auto s = loader.open();
        if (!s) { log::error("{}", s.error().message); return 1; }
        s = loader.configure(bc);
        if (!s) { log::error("{}", s.error().message); return 1; }
        s = loader.load();
        if (!s) { log::error("{}", s.error().message); return 1; }
        s = loader.attach();
        if (!s) { log::error("{}", s.error().message); return 1; }
    }

    Aggregator agg;
    SkeletonBridge bridge(loader);
    Symbolizer sym;
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

    using namespace ftxui;

    int tab_index = 0;
    auto tab_names = std::vector<std::string>{
        " Overview ", " Processes ", " Threads ", " Syscalls ", " Stacks ", " Diag "
    };
    auto tab_toggle = Toggle(&tab_names, &tab_index);

    auto screen = ScreenInteractive::Fullscreen();
    auto start_time = std::chrono::steady_clock::now();

    std::atomic<bool> refresh_running{true};
    std::thread refresh_thread([&] {
        while (refresh_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            screen.PostEvent(ftxui::Event::Custom);
        }
    });

    auto renderer = Renderer(tab_toggle, [&] {
        collector.poll(0);

        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        auto uptime = std::format("{}:{:02d}:{:02d}", secs / 3600, (secs % 3600) / 60, secs % 60);

        Element content;
        switch (tab_index) {
            case 0: content = render_overview(agg, collector, cfg_); break;
            case 1: content = render_processes(agg, cfg_); break;
            case 2: content = render_threads(agg, cfg_); break;
            case 3: content = render_syscalls(agg, cfg_); break;
            case 4: content = render_hot_stacks(agg, bridge, sym, cfg_); break;
            case 5: content = render_diagnostics(agg, collector, bridge); break;
            default: content = text("Unknown screen");
        }

        auto title_bar = hbox({
            text(" KernelScope ") | bold | bgcolor(Color::Blue) | color(Color::White),
            text(" "),
            text(std::string(kVersion)) | dim,
            filler(),
            text(std::to_string(agg.total_events())) | bold,
            text(" events") | dim,
            text("  "),
            text(std::to_string(collector.events_lost())) |
                (collector.events_lost() > 0 ? color(Color::Red) | bold : dim),
            text(" lost") | dim,
            text("  "),
            text(uptime) | dim,
            text(" "),
        });

        auto tab_bar = hbox({});
        for (int i = 0; i < static_cast<int>(tab_names.size()); i++) {
            auto label = text(tab_names[i]);
            if (i == tab_index)
                label = label | bold | bgcolor(Color::Cyan) | color(Color::Black);
            else
                label = label | dim;
            tab_bar = hbox({tab_bar, label, text(" ")});
        }

        auto status_bar = hbox({
            text(" q") | bold | color(Color::Cyan),
            text(":quit") | dim,
            text("  "),
            text("1-6") | bold | color(Color::Cyan),
            text(":screen") | dim,
            text("  "),
            text("←→") | bold | color(Color::Cyan),
            text(":navigate") | dim,
            filler(),
            cfg_.pid > 0
                ? hbox({text("PID:") | dim, text(std::to_string(cfg_.pid)) | bold, text(" ")})
                : text(""),
            !cfg_.comm.empty()
                ? hbox({text("COMM:") | dim, text(cfg_.comm) | bold, text(" ")})
                : text(""),
        });

        return vbox({
            title_bar,
            tab_bar | center,
            separator(),
            content | flex,
            separator(),
            status_bar,
        });
    });

    auto component = CatchEvent(renderer, [&](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        for (int i = 0; i < 6; i++) {
            if (event == ftxui::Event::Character(static_cast<char>('1' + i))) {
                tab_index = i;
                return true;
            }
        }
        return false;
    });

    screen.Loop(component);

    refresh_running = false;
    refresh_thread.join();
    collector.stop();
    return 0;
}

}
