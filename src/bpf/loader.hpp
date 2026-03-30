/*
 * loader.hpp — BPF skeleton lifecycle management for KernelScope.
 *
 * Wraps the libbpf skeleton open/load/attach/destroy cycle and
 * exposes configuration knobs (target pid, thresholds, stack flags)
 * via the skeleton's global variables.
 */

#pragma once

#include "util/errors.hpp"

#include <cstdint>
#include <memory>

struct kscope_bpf;

namespace kscope {

struct BpfConfig {
    uint32_t target_pid          = 0;
    uint64_t min_latency_ns      = 100'000;
    uint64_t min_offcpu_ns       = 1'000'000;
    bool     capture_user_stacks = true;
    bool     capture_kern_stacks = false;
    uint32_t stack_sampling_pct  = 100;
    std::string target_comm;
};

class BpfLoader {
public:
    BpfLoader();
    ~BpfLoader();

    BpfLoader(const BpfLoader&) = delete;
    BpfLoader& operator=(const BpfLoader&) = delete;
    BpfLoader(BpfLoader&&) noexcept;
    BpfLoader& operator=(BpfLoader&&) noexcept;

    Status open();
    Status configure(const BpfConfig& cfg);
    Status load();
    Status attach();
    void   detach();
    void   destroy();

    struct kscope_bpf* skeleton() const { return skel_; }

    int ringbuf_fd() const;
    int stack_trace_fd() const;

private:
    struct kscope_bpf* skel_ = nullptr;
};

}
