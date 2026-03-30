# KernelScope Architecture

## Overview

KernelScope is a low-overhead latency profiler for Linux processes.  It
combines an in-kernel eBPF data plane with a C++23 user-space control
plane to answer five questions in real time:

1. **Which** process/thread loses the most time?
2. **Where** is time lost — CPU, off-CPU, syscall, I/O, lock?
3. **Why** — which stack trace corresponds to the slowest paths?
4. **How much** — per-process, per-syscall, per-stack statistics with
   histograms and quantiles.
5. **How to export** — JSON snapshots, OpenTelemetry-compatible
   summaries, and Prometheus-style metrics.

## Component Diagram

```text
┌─────────────────────────────────────────────────────────────┐
│                       Linux Kernel                          │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ sys_enter/   │  │ sched_switch │  │ process_exec │      │
│  │ sys_exit     │  │              │  │ process_exit │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                 │               │
│  ┌──────┴─────────────────┴─────────────────┴──────┐       │
│  │                kscope.bpf.c                      │       │
│  │  active_syscalls map  │  sched_state map         │       │
│  │  stack_traces map     │  ring buffer → userspace │       │
│  └──────────────────────────────────────────────────┘       │
└──────────────────────────┬──────────────────────────────────┘
                           │ ring buffer
┌──────────────────────────┴──────────────────────────────────┐
│                     User Space (C++23)                       │
│                                                              │
│  ┌────────────┐   ┌────────────┐   ┌──────────────┐        │
│  │  Collector  │──▶│  Decoder   │──▶│  Classifier  │        │
│  │  (ringbuf)  │   │            │   │              │        │
│  └────────────┘   └────────────┘   └──────┬───────┘        │
│                                           │                 │
│       ┌───────────────────────────────────┤                 │
│       ▼                                   ▼                 │
│  ┌────────────┐                    ┌────────────┐           │
│  │ Aggregator  │                    │  Recorder   │          │
│  │ (windows,   │                    │  (.ksr)     │          │
│  │  histograms)│                    └────────────┘           │
│  └──────┬─────┘                                             │
│         │                                                    │
│  ┌──────┴───────┬──────────────┬──────────────┐             │
│  ▼              ▼              ▼              ▼              │
│  CLI           TUI          Export        Symbolizer        │
│  (top/trace)   (FTXUI)      (JSON/OTel/   (ELF/kallsyms)   │
│                              Prometheus)                     │
└──────────────────────────────────────────────────────────────┘
```

## Data Flow

### Live Mode

1. **BPF tracepoints** fire on syscall enter/exit, scheduler switch,
   and process lifecycle events.
2. **BPF handlers** compute deltas, apply threshold filters, capture
   stack IDs, and submit events to a **ring buffer** (16 MB default).
3. **Collector** polls the ring buffer via `ring_buffer__poll()`.
4. **Event decoder** converts the raw BPF struct into a typed C++
   `Event`.
5. **Classifier** assigns a `LatencyClass` — one of `on_cpu`,
   `off_cpu`, `syscall_blocked`, `io_wait`, `lock_wait`,
   `unknown_wait`.
6. **Aggregator** accumulates per-process, per-thread, per-syscall,
   and per-stack statistics with log₂ histograms.
7. **Display layer** (CLI, TUI, or exporter) reads aggregated data
   and presents it.

### Record / Replay

- **Record** intercepts events after the decoder and writes them to
  a `.ksr` binary file.
- **Replay** reads `.ksr` events and feeds them into the same
  aggregator/classifier pipeline — no root, no BPF, no kernel
  required.

## BPF Design Principles

- **Short handlers.**  The BPF verifier rejects complex programs.
  Each tracepoint handler does the minimum: timestamp, delta
  computation, threshold check, ring buffer submit.
- **Classification in user space.**  Syscall group taxonomy, futex
  heuristics, and I/O categorisation happen after events arrive.
- **CO-RE + skeleton.**  A single compiled BPF object adapts to
  different kernel versions via BTF.  The skeleton embeds the
  bytecode into the user-space binary.
- **Stack capture on slow path only.**  `bpf_get_stackid()` is
  called only when an event exceeds the latency threshold and
  sampling permits, keeping overhead low.

## Memory Model

| Component           | Budget        |
|---------------------|---------------|
| Ring buffer (BPF)   | 16 MB         |
| Active syscalls map | ~4 MB max     |
| Sched state map     | ~4 MB max     |
| Stack trace map     | ~8 MB max     |
| User-space aggregator | < 100 MB    |
| **Total target**    | **< 150 MB**  |

## Thread Model

- **Main thread**: CLI dispatch, TUI render loop, or export.
- **Collector callback**: invoked from `ring_buffer__poll()` on the
  main thread — no separate consumer thread.
- **Aggregator**: protected by `std::mutex` for thread safety when
  TUI refresh and collector callback overlap.

## Build System

CMake drives the entire build:

1. `clang -target bpf -O2` compiles `kscope.bpf.c` → `.bpf.o`.
2. `bpftool gen skeleton` produces `kscope.skel.h`.
3. `g++` (or clang++) compiles C++23 sources, linking against libbpf,
   libelf, and zlib.
4. FetchContent pulls CLI11, toml++, nlohmann/json, FTXUI, and
   GoogleTest.
