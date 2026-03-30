/*
 * stack_formatter.hpp — Stack trace display formatting for KernelScope.
 *
 * Converts resolved stack frames into formatted strings for CLI/TUI output.
 */

#pragma once

#include "symbolization/symbolizer.hpp"

#include <string>
#include <vector>

namespace kscope {

std::string format_stack(const std::vector<ResolvedFrame>& frames);
std::string format_frame(const ResolvedFrame& frame, size_t index);

}
