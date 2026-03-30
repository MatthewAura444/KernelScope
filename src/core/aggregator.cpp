/*
 * aggregator.cpp — Full event aggregation for KernelScope.
 *
 * Processes each incoming event into four dimensions (process, thread,
 * syscall, stack) with histograms.  Provides sorted top-N queries for
 * each dimension.  All public methods are mutex-protected.
 */

#include "core/aggregator.hpp"

#include <algorithm>

namespace kscope {

Aggregator::Aggregator() = default;

void Aggregator::process_event(const Event& ev) {
    std::lock_guard lk(mu_);
    total_++;

    switch (ev.type) {
        case EventType::SyscallSlow:
        case EventType::IoWait:
        case EventType::LockWait:
            update_process(ev);
            update_thread(ev);
            update_syscall(ev);
            update_stack(ev);
            break;
        case EventType::OffcpuSlice:
            update_process(ev);
            update_thread(ev);
            update_stack(ev);
            break;
        default:
            break;
    }
}

void Aggregator::update_process(const Event& ev) {
    auto& p = procs_[ev.pid];
    p.pid  = ev.pid;
    p.comm = std::string(ev.comm_str());
    p.slow_event_count++;
    if (ev.duration_ns > p.max_latency_ns)
        p.max_latency_ns = ev.duration_ns;
    p.latency_hist.record(ev.duration_ns);

    if (ev.type == EventType::OffcpuSlice)
        p.total_offcpu_ns += ev.duration_ns;
    else
        p.total_syscall_ns += ev.duration_ns;
}

void Aggregator::update_thread(const Event& ev) {
    uint64_t key = (static_cast<uint64_t>(ev.pid) << 32) | ev.tid;
    auto& t  = threads_[key];
    t.pid    = ev.pid;
    t.tid    = ev.tid;
    t.comm   = std::string(ev.comm_str());
    t.slow_event_count++;
    if (ev.duration_ns > t.max_latency_ns)
        t.max_latency_ns = ev.duration_ns;
    t.latency_hist.record(ev.duration_ns);

    if (ev.type == EventType::OffcpuSlice)
        t.total_offcpu_ns += ev.duration_ns;
    else
        t.total_syscall_ns += ev.duration_ns;
}

void Aggregator::update_syscall(const Event& ev) {
    auto& s = syscalls_[ev.syscall_id];
    s.syscall_id = ev.syscall_id;
    s.count++;
    s.total_ns += ev.duration_ns;
    if (ev.duration_ns > s.max_ns)
        s.max_ns = ev.duration_ns;
    s.latency_hist.record(ev.duration_ns);
    s.affected_pids.insert(ev.pid);
}

void Aggregator::update_stack(const Event& ev) {
    auto add = [&](int64_t id, bool kern) {
        if (id < 0) return;
        auto& st = stacks_[id];
        st.stack_id   = id;
        st.total_ns  += ev.duration_ns;
        st.count++;
        st.is_kernel  = kern;
        if (st.pid == 0) st.pid = ev.pid;
        if (st.representative_type == EventType::None)
            st.representative_type = ev.type;
    };
    add(ev.user_stack_id, false);
    add(ev.kern_stack_id, true);
}

void Aggregator::reset() {
    std::lock_guard lk(mu_);
    procs_.clear();
    threads_.clear();
    syscalls_.clear();
    stacks_.clear();
    total_ = 0;
}

uint64_t Aggregator::total_events() const {
    std::lock_guard lk(mu_);
    return total_;
}

std::vector<ProcessStats> Aggregator::top_processes(size_t n, SortBy by) const {
    std::lock_guard lk(mu_);
    std::vector<ProcessStats> v;
    v.reserve(procs_.size());
    for (auto& [_, p] : procs_) v.push_back(p);

    auto cmp = [by](const ProcessStats& a, const ProcessStats& b) {
        switch (by) {
            case SortBy::TotalLatency:
                return (a.total_syscall_ns + a.total_offcpu_ns) >
                       (b.total_syscall_ns + b.total_offcpu_ns);
            case SortBy::OffcpuTime:
                return a.total_offcpu_ns > b.total_offcpu_ns;
            case SortBy::MaxLatency:
                return a.max_latency_ns > b.max_latency_ns;
            case SortBy::EventCount:
                return a.slow_event_count > b.slow_event_count;
        }
        return false;
    };
    std::sort(v.begin(), v.end(), cmp);
    if (v.size() > n) v.resize(n);
    return v;
}

std::vector<ThreadStats> Aggregator::top_threads(size_t n, SortBy by) const {
    std::lock_guard lk(mu_);
    std::vector<ThreadStats> v;
    v.reserve(threads_.size());
    for (auto& [_, t] : threads_) v.push_back(t);

    auto cmp = [by](const ThreadStats& a, const ThreadStats& b) {
        switch (by) {
            case SortBy::TotalLatency:
                return (a.total_syscall_ns + a.total_offcpu_ns) >
                       (b.total_syscall_ns + b.total_offcpu_ns);
            case SortBy::OffcpuTime:
                return a.total_offcpu_ns > b.total_offcpu_ns;
            case SortBy::MaxLatency:
                return a.max_latency_ns > b.max_latency_ns;
            case SortBy::EventCount:
                return a.slow_event_count > b.slow_event_count;
        }
        return false;
    };
    std::sort(v.begin(), v.end(), cmp);
    if (v.size() > n) v.resize(n);
    return v;
}

std::vector<SyscallStats> Aggregator::top_syscalls(size_t n) const {
    std::lock_guard lk(mu_);
    std::vector<SyscallStats> v;
    v.reserve(syscalls_.size());
    for (auto& [_, s] : syscalls_) v.push_back(s);

    std::sort(v.begin(), v.end(), [](const SyscallStats& a, const SyscallStats& b) {
        return a.total_ns > b.total_ns;
    });
    if (v.size() > n) v.resize(n);
    return v;
}

std::vector<StackStats> Aggregator::top_stacks(size_t n) const {
    std::lock_guard lk(mu_);
    std::vector<StackStats> v;
    v.reserve(stacks_.size());
    for (auto& [_, s] : stacks_) v.push_back(s);

    std::sort(v.begin(), v.end(), [](const StackStats& a, const StackStats& b) {
        return a.total_ns > b.total_ns;
    });
    if (v.size() > n) v.resize(n);
    return v;
}

}
