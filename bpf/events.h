/*
 * events.h — Shared event definitions for KernelScope.
 *
 * This header is included by both BPF programs (kscope.bpf.c) and
 * C++ user-space code. It defines the wire format of events transmitted
 * through the ring buffer, the event-type enum, task identification
 * keys, and the active-syscall tracking structure.
 *
 * Keep this file C-compatible (no C++ constructs) so clang can compile
 * it for the BPF target.
 */

#ifndef KSCOPE_EVENTS_H
#define KSCOPE_EVENTS_H

#ifdef __cplusplus
#include <cstdint>
#include <linux/types.h>
#else
#include "vmlinux.h"
#endif

#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif

enum event_type {
    EV_NONE            = 0,
    EV_SYSCALL_SLOW    = 1,
    EV_OFFCPU_SLICE    = 2,
    EV_LOCK_WAIT       = 3,
    EV_IO_WAIT         = 4,
    EV_PROC_EXEC       = 5,
    EV_PROC_EXIT       = 6,
    EV_LOST_SAMPLES    = 7,
    EV_DIAG            = 8,
};

enum latency_class {
    LAT_UNKNOWN        = 0,
    LAT_ON_CPU         = 1,
    LAT_OFF_CPU        = 2,
    LAT_SYSCALL_BLOCKED = 3,
    LAT_IO_WAIT        = 4,
    LAT_LOCK_WAIT      = 5,
    LAT_UNKNOWN_WAIT   = 6,
};

struct task_key {
    __u32 pid;
    __u32 tid;
};

struct active_syscall {
    __u64 id;
    __u64 start_ns;
    __s64 user_stack_id;
    __s64 kern_stack_id;
    __u32 flags;
    __u32 pad;
};

struct kscope_sched_state {
    __u64 switch_out_ns;
    __u64 switch_in_ns;
    __u64 accumulated_offcpu_ns;
    __u32 last_state;
    __u32 pad;
};

struct event {
    __u64 timestamp_ns;
    __u32 pid;
    __u32 tid;
    char  comm[TASK_COMM_LEN];
    __u32 cpu;
    enum event_type type;
    enum latency_class classification;
    __u64 duration_ns;
    __u64 syscall_id;
    __s64 user_stack_id;
    __s64 kern_stack_id;
    __u32 flags;
    __u32 pad;
};

#define EVF_HAS_USER_STACK  (1U << 0)
#define EVF_HAS_KERN_STACK  (1U << 1)
#define EVF_FILTERED        (1U << 2)
#define EVF_TRUNCATED       (1U << 3)

#define DEFAULT_RINGBUF_SIZE  (16U * 1024U * 1024U)

#define MAX_ACTIVE_SYSCALLS   65536
#define MAX_SCHED_ENTRIES    65536
#define MAX_STACK_DEPTH      127

#endif /* KSCOPE_EVENTS_H */
