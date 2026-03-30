/*
 * api.hpp — Public constants and version info for KernelScope.
 *
 * Single include for version macros, default configuration values,
 * and any identifiers exposed beyond the internal implementation.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace kscope {

inline constexpr std::string_view kVersion    = "1.0.0-dev";
inline constexpr std::string_view kName       = "KernelScope";
inline constexpr std::string_view kBinaryName = "kscope";

inline constexpr uint32_t kMinKernelMajor = 6;
inline constexpr uint32_t kMinKernelMinor = 1;

inline constexpr uint64_t kDefaultMinLatencyUs  = 100;
inline constexpr uint64_t kDefaultMinOffcpuUs   = 1000;
inline constexpr uint32_t kDefaultWindowSec      = 5;
inline constexpr uint32_t kDefaultTopN           = 20;
inline constexpr uint32_t kDefaultRingbufSizeMb  = 16;
inline constexpr uint32_t kDefaultStackSamplingPct = 100;

inline constexpr std::string_view kConfigFileName = "kscope.toml";

inline constexpr std::string_view kKsrMagic    = "KSCOPE\x01\x00";
inline constexpr std::string_view kKsrFooter   = "KSFOOT\x01\x00";
inline constexpr uint32_t kKsrVersion          = 1;

}
