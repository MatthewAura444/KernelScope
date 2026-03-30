/*
 * kscope.bpf.c — BPF programs for KernelScope.
 *
 * Contains all tracepoint handlers attached by KernelScope:
 *   - raw_syscalls/sys_enter  — record syscall entry timestamp
 *   - raw_syscalls/sys_exit   — compute latency, emit EV_SYSCALL_SLOW
 *   - sched/sched_switch      — track off-CPU time
 *   - sched/sched_process_exec — track process creation
 *   - sched/sched_process_exit — cleanup and notification
 *
 * Design principles:
 *   - Keep BPF handlers short and simple for verifier happiness.
 *   - Heavy classification and aggregation is done in user-space.
 *   - All events go through the ring buffer (BPF_MAP_TYPE_RINGBUF).
 *   - CO-RE relocations via vmlinux.h for kernel portability.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#include "events.h"
#include "maps.h"
#include "filters.h"

char LICENSE[] SEC("license") = "GPL";

SEC("tracepoint/raw_syscalls/sys_enter")
int tp_sys_enter(struct trace_event_raw_sys_enter *ctx)
{
    struct task_key key;
    fill_task_key(&key);

    if (!filter_pid(key.pid))
        return 0;

    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(&comm, sizeof(comm));
    if (!filter_comm(comm))
        return 0;

    struct active_syscall entry = {};
    entry.id = ctx->id;
    entry.start_ns = bpf_ktime_get_ns();
    entry.user_stack_id = -1;
    entry.kern_stack_id = -1;
    entry.flags = 0;

    if (should_capture_stack()) {
        if (capture_user_stacks)
            entry.user_stack_id =
                bpf_get_stackid(ctx, &stack_traces, BPF_F_USER_STACK);
        if (capture_kern_stacks)
            entry.kern_stack_id =
                bpf_get_stackid(ctx, &stack_traces, 0);
        if (entry.user_stack_id >= 0)
            entry.flags |= EVF_HAS_USER_STACK;
        if (entry.kern_stack_id >= 0)
            entry.flags |= EVF_HAS_KERN_STACK;
    }

    bpf_map_update_elem(&active_syscalls, &key, &entry, BPF_ANY);
    return 0;
}

SEC("tracepoint/raw_syscalls/sys_exit")
int tp_sys_exit(struct trace_event_raw_sys_exit *ctx)
{
    struct task_key key;
    fill_task_key(&key);

    struct active_syscall *entry =
        bpf_map_lookup_elem(&active_syscalls, &key);
    if (!entry)
        return 0;

    __u64 now = bpf_ktime_get_ns();
    __u64 delta_ns = now - entry->start_ns;

    if (delta_ns >= min_latency_ns) {
        struct event *e =
            bpf_ringbuf_reserve(&events, sizeof(*e), 0);
        if (e) {
            fill_event_common(e);
            e->timestamp_ns = now;
            e->type = EV_SYSCALL_SLOW;
            e->duration_ns = delta_ns;
            e->syscall_id = entry->id;
            e->user_stack_id = entry->user_stack_id;
            e->kern_stack_id = entry->kern_stack_id;
            e->flags = entry->flags;
            bpf_ringbuf_submit(e, 0);
        }
    }

    bpf_map_delete_elem(&active_syscalls, &key);
    return 0;
}

SEC("tracepoint/sched/sched_switch")
int tp_sched_switch(struct trace_event_raw_sched_switch *ctx)
{
    __u32 prev_tid = ctx->prev_pid;
    __u32 next_tid = ctx->next_pid;

    struct task_key prev_key = { .pid = prev_tid, .tid = prev_tid };
    if (filter_pid(prev_tid)) {
        struct kscope_sched_state new_state = {};
        struct kscope_sched_state *existing =
            bpf_map_lookup_elem(&sched_states, &prev_key);
        if (existing)
            new_state = *existing;

        new_state.switch_out_ns = bpf_ktime_get_ns();
        new_state.last_state = ctx->prev_state;
        bpf_map_update_elem(&sched_states, &prev_key, &new_state,
                            BPF_ANY);
    }

    struct task_key next_key = { .pid = next_tid, .tid = next_tid };
    if (filter_pid(next_tid)) {
        struct kscope_sched_state *state =
            bpf_map_lookup_elem(&sched_states, &next_key);
        if (state && state->switch_out_ns > 0) {
            __u64 now = bpf_ktime_get_ns();
            __u64 offcpu_ns = now - state->switch_out_ns;

            state->switch_in_ns = now;
            state->accumulated_offcpu_ns += offcpu_ns;

            if (offcpu_ns >= min_offcpu_ns) {
                struct event *e =
                    bpf_ringbuf_reserve(&events, sizeof(*e), 0);
                if (e) {
                    e->timestamp_ns = now;
                    e->pid = next_tid;
                    e->tid = next_tid;
                    e->cpu = bpf_get_smp_processor_id();
                    bpf_probe_read_kernel_str(
                        &e->comm, sizeof(e->comm),
                        ctx->next_comm);
                    e->type = EV_OFFCPU_SLICE;
                    e->duration_ns = offcpu_ns;
                    e->classification = LAT_OFF_CPU;
                    e->syscall_id = 0;
                    e->user_stack_id = -1;
                    e->kern_stack_id = -1;
                    e->flags = 0;
                    e->pad = 0;
                    bpf_ringbuf_submit(e, 0);
                }
            }
            state->switch_out_ns = 0;
        }
    }

    return 0;
}

SEC("tracepoint/sched/sched_process_exec")
int tp_sched_process_exec(struct trace_event_raw_sched_process_exec *ctx)
{
    struct task_key key;
    fill_task_key(&key);

    if (!filter_pid(key.pid))
        return 0;

    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e) {
        fill_event_common(e);
        e->type = EV_PROC_EXEC;
        e->duration_ns = 0;
        e->syscall_id = 0;
        bpf_ringbuf_submit(e, 0);
    }
    return 0;
}

SEC("tracepoint/sched/sched_process_exit")
int tp_sched_process_exit(struct trace_event_raw_sched_process_template *ctx)
{
    struct task_key key;
    fill_task_key(&key);

    bpf_map_delete_elem(&active_syscalls, &key);
    bpf_map_delete_elem(&sched_states, &key);

    if (!filter_pid(key.pid))
        return 0;

    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e) {
        fill_event_common(e);
        e->type = EV_PROC_EXIT;
        e->duration_ns = 0;
        e->syscall_id = 0;
        bpf_ringbuf_submit(e, 0);
    }
    return 0;
}
