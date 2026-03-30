# KernelScope Performance Characteristics

## Design Goals

| Metric                  | Target       |
|-------------------------|--------------|
| Overhead (moderate load)| < 2–3%       |
| User-space RSS          | < 150 MB     |
| Startup time            | < 1 second   |
| Stable session          | 10–30 min    |

## Overhead Sources

### BPF Side

1. **Tracepoint entry cost.**  Each raw_syscalls tracepoint
   invocation adds ~200–500 ns.  For a process making 100k
   syscalls/sec, this is ≈ 0.05 ms/sec — negligible.

2. **Hash map lookups.**  `active_syscalls` and `sched_states` use
   hash maps.  Lookup is O(1) amortised with a small constant.

3. **Stack capture.**  `bpf_get_stackid()` is the most expensive
   helper, costing ~1–5 µs per call.  KernelScope mitigates this
   by only capturing stacks when:
   - The event exceeds the latency threshold.
   - The sampling ratio permits (`--stack-sampling`).
   - The relevant flag is enabled (`--user-stacks`, `--kernel-stacks`).

4. **Ring buffer submission.**  `bpf_ringbuf_reserve()` +
   `bpf_ringbuf_submit()` is faster than perf event output and
   preserves cross-CPU ordering.

### User-Space Side

1. **Ring buffer polling.**  `ring_buffer__poll()` with a 100 ms
   timeout.  Batch processing of buffered events.

2. **Aggregation.**  O(1) per event: hash map insertion + histogram
   bucket update.  Histograms use log₂ buckets for O(1) insertion.

3. **Symbolisation.**  Lazy resolution with LRU-bounded ELF cache.
   `/proc/pid/maps` is re-read only on cache miss.  Kernel symbols
   loaded once from `/proc/kallsyms`.

## Memory Budget

| Component             | Estimated Size |
|-----------------------|----------------|
| Ring buffer           | 16 MB          |
| Active syscalls map   | ~4 MB          |
| Sched state map       | ~4 MB          |
| Stack trace map       | ~8 MB          |
| Aggregator (in-memory)| 10–50 MB       |
| ELF symbol cache      | 5–20 MB        |
| **Total**             | **47–102 MB**  |

## Benchmark Scenarios

The following scenarios are provided in `scripts/bench.sh`:

### 1. Idle System
Measures baseline overhead when no target processes are active.
Expected: near-zero CPU, < 30 MB RSS.

### 2. Syscall-Heavy Workload
Uses `examples/cpu_stress` with short sleep intervals, generating
thousands of nanosleep syscalls per second.

### 3. I/O-Heavy Workload
Uses `examples/io_stress` with large block sizes and frequent
fsync, generating sustained write/read/fsync events.

### 4. Thread Contention
Uses `examples/futex_stress` with many threads, creating high
futex wait rates.

### 5. Many Short-Lived Processes
Fork-exec loop creating and destroying processes rapidly.  Tests
process exec/exit tracking overhead and map cleanup.

## Tuning Knobs

| Parameter               | Flag                    | Effect                    |
|-------------------------|-------------------------|---------------------------|
| Minimum latency threshold | `--min-latency-us`    | Filters slow syscalls     |
| Minimum off-CPU threshold | `--min-offcpu-us`     | Filters short off-CPU     |
| Stack sampling rate     | `--stack-sampling`      | 0–100% of eligible events |
| User stacks             | `--user-stacks`         | Enable/disable            |
| Kernel stacks           | `--kernel-stacks`       | Enable/disable            |
| Ring buffer size        | `kscope.toml:ringbuf.size_mb` | Ring buffer capacity |
| Window duration         | `--window`              | Aggregation reset period  |

## Ring Buffer Overflow

When the ring buffer fills faster than user space can drain it,
events are silently dropped.  KernelScope detects this via the
ring buffer callback's lost-event counter and reports it in:

- CLI footer: `[events: N received, M lost]`
- TUI diagnostics screen
- JSON export `diagnostics.events_lost`
- Prometheus metric `kscope_events_lost_total`

**Mitigation:** Increase `ringbuf.size_mb` in `kscope.toml` or
raise the minimum latency threshold to reduce event volume.
