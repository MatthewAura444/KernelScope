/*
 * stack_formatter.cpp — Stack trace formatting for KernelScope.
 */

#include "symbolization/stack_formatter.hpp"

#include <format>

namespace kscope {

std::string format_frame(const ResolvedFrame& frame, size_t index) {
    if (frame.resolved)
        return std::format("  #{} {} ({}+0x{:x})",
                           index, frame.symbol, frame.binary, frame.offset);
    return std::format("  #{} 0x{:x}", index, frame.addr);
}

std::string format_stack(const std::vector<ResolvedFrame>& frames) {
    std::string result;
    for (size_t i = 0; i < frames.size(); i++) {
        result += format_frame(frames[i], i);
        result += '\n';
    }
    return result;
}

}
