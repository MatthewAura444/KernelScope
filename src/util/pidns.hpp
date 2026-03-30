/*
 * pidns.hpp — PID namespace translation utilities for KernelScope.
 *
 * BPF programs see kernel-level (init namespace) PIDs via
 * bpf_get_current_pid_tgid(), but /proc is mounted in the
 * local PID namespace.  On systems with PID namespaces (e.g.
 * containers, WSL2), these PIDs differ.
 *
 * This module provides translation between namespace-local PIDs
 * (as shown by ps/top) and kernel PIDs (as seen by BPF).
 */

#pragma once

#include <cstdint>
#include <optional>

namespace kscope::pidns {

uint32_t to_kernel_pid(uint32_t ns_pid);
uint32_t to_ns_pid(uint32_t kernel_pid);
bool has_pid_namespace();

}
