/*
 * filters.h — BPF-side filtering helpers for KernelScope.
 *
 * Provides lightweight inline functions that BPF programs call at the
 * very top of each tracepoint handler to decide whether the event
 * belongs to a tracked task.  Keeping filters here avoids duplicating
 * branching logic across handlers and makes the verifier's job easier.
 */

#ifndef KSCOPE_FILTERS_H
#define KSCOPE_FILTERS_H

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "maps.h"

static __always_inline bool filter_pid(__u32 pid)
{
    if (target_pid == 0)
        return true;
    return pid == target_pid;
}

static __always_inline bool filter_comm(const char *comm)
{
    if (target_comm[0] == '\0')
        return true;

    for (int i = 0; i < TASK_COMM_LEN; i++) {
        if (target_comm[i] != comm[i])
            return false;
        if (target_comm[i] == '\0')
            return true;
    }
    return true;
}

static __always_inline bool should_capture_stack(void)
{
    if (stack_sampling_pct >= 100)
        return true;
    if (stack_sampling_pct == 0)
        return false;

    __u32 rand = bpf_get_prandom_u32();
    return (rand % 100) < stack_sampling_pct;
}

static __always_inline void get_ns_pid_tgid(__u32 *pid_out, __u32 *tid_out)
{
    if (pidns_dev != 0 && pidns_ino != 0) {
        struct bpf_pidns_info ns = {};
        if (bpf_get_ns_current_pid_tgid(pidns_dev, pidns_ino,
                                         &ns, sizeof(ns)) == 0) {
            *pid_out = ns.tgid;
            *tid_out = ns.pid;
            return;
        }
    }
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    *pid_out = pid_tgid >> 32;
    *tid_out = (__u32)pid_tgid;
}

static __always_inline void fill_task_key(struct task_key *key)
{
    get_ns_pid_tgid(&key->pid, &key->tid);
}

static __always_inline void fill_event_common(struct event *e)
{
    get_ns_pid_tgid(&e->pid, &e->tid);
    e->timestamp_ns = bpf_ktime_get_ns();
    e->cpu = bpf_get_smp_processor_id();
    bpf_get_current_comm(&e->comm, sizeof(e->comm));
    e->user_stack_id = -1;
    e->kern_stack_id = -1;
    e->flags = 0;
    e->classification = LAT_UNKNOWN;
}

#endif /* KSCOPE_FILTERS_H */
