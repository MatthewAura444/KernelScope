/*
 * main.cpp — Entry point for KernelScope.
 *
 * Parses CLI arguments, sets up logging, installs signal handlers for
 * graceful shutdown (SIGINT/SIGTERM), and dispatches to the command.
 */

#include <csignal>
#include <cstdlib>

#include "cli/commands.hpp"
#include "cli/parse_args.hpp"
#include "kscope/api.hpp"
#include "util/logging.hpp"
#include "util/signal.hpp"

namespace {
void signal_handler(int) {
    kscope::request_shutdown();
}
}

int main(int argc, char* argv[]) {
    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    kscope::Config cfg = kscope::parse_args(argc, argv);

    if (cfg.quiet)
        kscope::log::set_level(kscope::log::Level::Error);
    else if (cfg.verbose)
        kscope::log::set_level(kscope::log::Level::Debug);

    return kscope::run_command(cfg);
}
