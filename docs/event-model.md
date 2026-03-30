# KernelScope Event Model

## Event Types

| Enum                 | Value | Description                                    |
|----------------------|-------|------------------------------------------------|
| `EV_NONE`            | 0     | Unused / uninitialised                         |
| `EV_SYSCALL_SLOW`    | 1     | Syscall exceeded latency threshold             |
| `EV_OFFCPU_SLICE`    | 2     | Thread spent time off-CPU above threshold      |
| `EV_LOCK_WAIT`       | 3     | Futex wait detected as lock contention         |
| `EV_IO_WAIT`         | 4     | I/O syscall exceeded threshold                 |
| `EV_PROC_EXEC`       | 5     | New process executed (sched_process_exec)       |
| `EV_PROC_EXIT`       | 6     | Process exited (sched_process_exit)             |
| `EV_LOST_SAMPLES`    | 7     | Ring buffer overflow indicator                 |
| `EV_DIAG`            | 8     | Internal diagnostic event                      |

## Event Structure (Wire Format)

```c
struct event {
    __u64 timestamp_ns;       // ktime_get_ns() at event emission
    __u32 pid;                // Process ID (tgid)
    __u32 tid;                // Thread ID (pid in kernel terms)
    char  comm[16];           // Process name
    __u32 cpu;                // CPU where event occurred
    enum  event_type type;    // Event category
    enum  latency_class classification;  // Latency bucket
    __u64 duration_ns;        // Duration of the slow operation
    __u64 syscall_id;         // x86_64 syscall number
    __s64 user_stack_id;      // User-space stack trace ID (-1 if none)
    __s64 kern_stack_id;      // Kernel stack trace ID (-1 if none)
    __u32 flags;              // Bitfield (see below)
    __u32 pad;                // Alignment padding
};
```

## Flag Bits

| Flag                  | Bit | Meaning                              |
|-----------------------|-----|--------------------------------------|
| `EVF_HAS_USER_STACK`  | 0   | `user_stack_id` is valid             |
| `EVF_HAS_KERN_STACK`  | 1   | `kern_stack_id` is valid             |
| `EVF_FILTERED`        | 2   | Event matched an active filter       |
| `EVF_TRUNCATED`       | 3   | Event was truncated (future use)     |

## Latency Classification

Every event with a non-zero duration is classified into one of six
categories.  Classification is **deterministic** and happens in user
space after the event arrives from the ring buffer.

| Class              | Rule                                               |
|--------------------|----------------------------------------------------|
| `on_cpu`           | Thread is scheduled and running (no slow event)    |
| `off_cpu`          | `EV_OFFCPU_SLICE` from sched_switch                |
| `syscall_blocked`  | Slow syscall not in I/O or lock group              |
| `io_wait`          | Slow syscall in FileIO / Sync / Network group      |
| `lock_wait`        | Slow `futex` syscall                               |
| `unknown_wait`     | Slow event not matching any known group            |

## Syscall Group Taxonomy

| Group     | Syscalls (x86_64 IDs)                                  |
|-----------|--------------------------------------------------------|
| FileIO    | read(0), write(1), open(2), close(3), pread64(17), pwrite64(18), openat(257) |
| Sync      | fsync(74), fdatasync(75)                               |
| Network   | socket(41), connect(42), accept(43), sendto(44), recvfrom(45), sendmsg(46), recvmsg(47), bind(49), listen(50), accept4(288) |
| Lock      | futex(202)                                             |
| Process   | clone(56), fork(57), execve(59), wait4(61)             |
| Memory    | mmap(9), mprotect(10), munmap(11), brk(12)             |
| Signal    | (reserved for future expansion)                        |
| Other     | All remaining syscalls                                 |

## BPF Maps

| Map               | Type                    | Key            | Value             | Max Entries |
|-------------------|-------------------------|----------------|-------------------|-------------|
| `active_syscalls` | `BPF_MAP_TYPE_HASH`     | `task_key`     | `active_syscall`  | 65536       |
| `sched_states`    | `BPF_MAP_TYPE_HASH`     | `task_key`     | `sched_state`     | 65536       |
| `events`          | `BPF_MAP_TYPE_RINGBUF`  | —              | —                 | 16 MB       |
| `stack_traces`    | `BPF_MAP_TYPE_STACK_TRACE` | stack ID    | `u64[127]`        | 16384       |

## Global Configuration Variables

These are set in user space before `bpf_object__load()` via the
skeleton's `.rodata` / `.bss` sections:

| Variable              | Type    | Default       |
|-----------------------|---------|---------------|
| `target_pid`          | `u32`   | 0 (all)       |
| `min_latency_ns`      | `u64`   | 100,000 (100µs) |
| `min_offcpu_ns`       | `u64`   | 1,000,000 (1ms) |
| `capture_user_stacks` | `u8`    | 1             |
| `capture_kern_stacks` | `u8`    | 0             |
| `stack_sampling_pct`  | `u8`    | 50            |
| `target_comm`         | `char[16]` | "" (all)   |
