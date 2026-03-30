/*
 * loader.cpp — BPF skeleton lifecycle for KernelScope.
 *
 * Implements open → configure → load → attach → destroy using
 * the generated kscope.skel.h.  Configuration is applied by writing
 * to the skeleton's rodata/data sections between open and load.
 */

#include "bpf/loader.hpp"

#include <bpf/libbpf.h>
#include <cstring>
#include <sys/stat.h>

#include "events.h"
#include "kscope.skel.h"
#include "util/logging.hpp"

namespace kscope {

BpfLoader::BpfLoader() = default;

BpfLoader::~BpfLoader() {
    destroy();
}

BpfLoader::BpfLoader(BpfLoader&& other) noexcept
    : skel_(other.skel_) {
    other.skel_ = nullptr;
}

BpfLoader& BpfLoader::operator=(BpfLoader&& other) noexcept {
    if (this != &other) {
        destroy();
        skel_ = other.skel_;
        other.skel_ = nullptr;
    }
    return *this;
}

Status BpfLoader::open() {
    skel_ = kscope_bpf__open();
    if (!skel_)
        return std::unexpected(
            make_error(ErrorCode::BpfLoadFailed, "kscope_bpf__open() failed"));

    log::info("BPF skeleton opened");
    return {};
}

Status BpfLoader::configure(const BpfConfig& cfg) {
    if (!skel_)
        return std::unexpected(
            make_error(ErrorCode::Internal, "skeleton not opened"));

    skel_->rodata->target_pid         = cfg.target_pid;
    skel_->rodata->min_latency_ns     = cfg.min_latency_ns;
    skel_->rodata->min_offcpu_ns      = cfg.min_offcpu_ns;
    skel_->rodata->capture_user_stacks = cfg.capture_user_stacks ? 1 : 0;
    skel_->rodata->capture_kern_stacks = cfg.capture_kern_stacks ? 1 : 0;
    skel_->rodata->stack_sampling_pct  = cfg.stack_sampling_pct;

    if (!cfg.target_comm.empty()) {
        size_t len = std::min(cfg.target_comm.size(), size_t{TASK_COMM_LEN - 1});
        std::memcpy(const_cast<char*>(skel_->rodata->target_comm),
                     cfg.target_comm.c_str(), len);
    }

    struct stat st;
    if (::stat("/proc/self/ns/pid", &st) == 0) {
        skel_->rodata->pidns_dev = static_cast<uint32_t>(st.st_dev);
        skel_->rodata->pidns_ino = static_cast<uint32_t>(st.st_ino);
        log::info("PID namespace: dev={} ino={}", st.st_dev, st.st_ino);
    }

    log::info("BPF config: pid={} min_lat={}ns min_offcpu={}ns",
              cfg.target_pid, cfg.min_latency_ns, cfg.min_offcpu_ns);
    return {};
}

Status BpfLoader::load() {
    if (!skel_)
        return std::unexpected(
            make_error(ErrorCode::Internal, "skeleton not opened"));

    int err = kscope_bpf__load(skel_);
    if (err)
        return std::unexpected(
            make_error(ErrorCode::BpfLoadFailed,
                       std::format("kscope_bpf__load() failed: {}", err)));

    log::info("BPF programs loaded into kernel");
    return {};
}

Status BpfLoader::attach() {
    if (!skel_)
        return std::unexpected(
            make_error(ErrorCode::Internal, "skeleton not opened"));

    int err = kscope_bpf__attach(skel_);
    if (err)
        return std::unexpected(
            make_error(ErrorCode::BpfAttachFailed,
                       std::format("kscope_bpf__attach() failed: {}", err)));

    log::info("BPF programs attached to tracepoints");
    return {};
}

void BpfLoader::detach() {
    if (skel_)
        kscope_bpf__detach(skel_);
}

void BpfLoader::destroy() {
    if (skel_) {
        kscope_bpf__destroy(skel_);
        skel_ = nullptr;
    }
}

int BpfLoader::ringbuf_fd() const {
    if (!skel_) return -1;
    return bpf_map__fd(skel_->maps.events);
}

int BpfLoader::stack_trace_fd() const {
    if (!skel_) return -1;
    return bpf_map__fd(skel_->maps.stack_traces);
}

}
