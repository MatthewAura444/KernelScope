/*
 * tables.hpp — Reusable FTXUI table components for KernelScope TUI.
 *
 * Converts aggregated stats vectors into FTXUI Element tables that
 * screens.cpp composes into full screen layouts.
 */

#pragma once

#include <ftxui/dom/elements.hpp>

#include "core/aggregator.hpp"

#include <vector>

namespace kscope {

ftxui::Element render_process_table(const std::vector<ProcessStats>& procs);
ftxui::Element render_syscall_table(const std::vector<SyscallStats>& scs);

}
