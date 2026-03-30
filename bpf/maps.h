/*
 * maps.h — BPF map definitions for KernelScope.
 *
 * Declares all BPF maps and global configuration variables used by
 * kscope.bpf.c.  This file is BPF-only (included by the BPF program,
 * not by user-space).  User-space accesses maps through the generated
 * skeleton and libbpf APIs.
 */

#ifndef KSCOPE_MAPS_H
#define KSCOPE_MAPS_H

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include "events.h"

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ACTIVE_SYSCALLS);
    __type(key, struct task_key);
    __type(value, struct active_syscall);
} active_syscalls SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_SCHED_ENTRIES);
    __type(key, struct task_key);
    __type(value, struct kscope_sched_state);
} sched_states SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, DEFAULT_RINGBUF_SIZE);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_STACK_TRACE);
    __uint(max_entries, 32768);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, MAX_STACK_DEPTH * sizeof(__u64));
} stack_traces SEC(".maps");

const volatile __u32 target_pid = 0;
const volatile __u64 min_latency_ns = 100000;
const volatile __u64 min_offcpu_ns = 1000000;
const volatile __u32 capture_user_stacks = 1;
const volatile __u32 capture_kern_stacks = 0;
const volatile __u32 stack_sampling_pct = 100;

const volatile __u32 pidns_dev = 0;
const volatile __u32 pidns_ino = 0;

const volatile char target_comm[TASK_COMM_LEN] = {};

#endif /* KSCOPE_MAPS_H */
