/*
 * aggregator.hpp — Event aggregation engine for KernelScope.
 *
 * Collects events and maintains per-process, per-thread, per-syscall,
 * and per-stack statistics.  Each dimension carries a Histogram for
 * P50/P95/P99 estimation.  Thread-safe: a mutex protects all mutations
 * so the collector callback and the display thread can coexist.
 */

#pragma once

#include "core/event.hpp"
#include "core/histograms.hpp"

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace kscope {

enum class SortBy { TotalLatency, OffcpuTime, MaxLatency, EventCount };

struct ThreadStats {
    uint32_t pid = 0;
    uint32_t tid = 0;
    std::string comm;
    uint64_t total_syscall_ns = 0;
    uint64_t total_offcpu_ns  = 0;
    uint64_t max_latency_ns   = 0;
    uint32_t slow_event_count = 0;
    Histogram latency_hist;
};

struct ProcessStats {
    uint32_t pid = 0;
    std::string comm;
    uint64_t total_syscall_ns = 0;
    uint64_t total_offcpu_ns  = 0;
    uint64_t max_latency_ns   = 0;
    uint32_t slow_event_count = 0;
    Histogram latency_hist;
};

struct SyscallStats {
    uint64_t syscall_id = 0;
    uint64_t count      = 0;
    uint64_t total_ns   = 0;
    uint64_t max_ns     = 0;
    Histogram latency_hist;
    std::set<uint32_t> affected_pids;
};

struct StackStats {
    int64_t  stack_id = 0;
    uint64_t total_ns = 0;
    uint64_t count    = 0;
    uint32_t pid      = 0;
    EventType representative_type = EventType::None;
    bool is_kernel = false;
};

class Aggregator {
public:
    Aggregator();

    void process_event(const Event& ev);
    void reset();

    std::vector<ProcessStats> top_processes(size_t n, SortBy by = SortBy::TotalLatency) const;
    std::vector<ThreadStats>  top_threads(size_t n, SortBy by = SortBy::TotalLatency) const;
    std::vector<SyscallStats> top_syscalls(size_t n) const;
    std::vector<StackStats>   top_stacks(size_t n) const;

    uint64_t total_events() const;
    const std::unordered_map<uint32_t, ProcessStats>& process_map() const { return procs_; }
    const std::unordered_map<uint64_t, SyscallStats>& syscall_map() const { return syscalls_; }

private:
    void update_process(const Event& ev);
    void update_thread(const Event& ev);
    void update_syscall(const Event& ev);
    void update_stack(const Event& ev);

    std::unordered_map<uint32_t, ProcessStats> procs_;
    std::unordered_map<uint64_t, ThreadStats>  threads_;
    std::unordered_map<uint64_t, SyscallStats> syscalls_;
    std::unordered_map<int64_t,  StackStats>   stacks_;
    uint64_t total_ = 0;
    mutable std::mutex mu_;
};

}
