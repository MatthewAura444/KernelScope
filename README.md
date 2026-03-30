<div align="center">

<br>

```
 ╻┏━ ╻ ╻   ┏━┓┏━╸┏━┓┏━┓┏━╸
 ┣┻┓ ┗━┫   ┗━┓┃  ┃ ┃┣━┛┣╸
 ╹ ╹   ╹   ┗━┛┗━╸┗━┛╹  ┗━╸
```

**eBPF latency profiler for Linux**

*Zero-instrumentation · Real-time · Single binary*

<br>

[![Linux](https://img.shields.io/badge/linux-6.1%2B-blue?logo=linux&logoColor=white)](https://kernel.org)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus)](https://isocpp.org)
[![eBPF](https://img.shields.io/badge/eBPF-CO--RE-orange)](https://ebpf.io)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

</div>

---

## What it does

KernelScope attaches to the kernel via eBPF and answers:

- **Who** is slow — processes and threads ranked by wasted time
- **Where** — CPU, off-CPU, syscall, I/O, lock contention
- **Why** — stack traces attributed to the slowest paths
- **Export** — JSON, OpenTelemetry spans, Prometheus metrics

No recompilation. No agents. No code changes. Just run it.

## Quick look

```
$ sudo kscope top --window 5

  PID      COMM                  SYSCALL       OFFCPU          MAX   EVENTS          P95
  ─── ────     ─────── ──────    ─── ──────    ───
  211      chronyd              751.35ms     751.14ms     250.48ms       10     201.33ms
  7039     kscope                 1.026s          0ns     100.27ms       44     100.66ms
  1467     kworker/2:0               0ns     511.92ms     411.97ms        3     100.66ms
  17       rcu_preempt               0ns     443.59ms     348.01ms        8      50.33ms
  1        mini_init             10.55ms          0ns      10.51ms        3      24.6µs

  [events: 112 received, 0 lost]
```

```
$ sudo kscope top-syscalls --duration 5

  SYSCALL             COUNT        TOTAL          AVG          P95          MAX   PIDS
  ─────── ───── ─────    ───    ───    ─── ────
  epoll_wait             24       1.100s      45.81ms     100.66ms     100.30ms      1
  pselect6                3     751.22ms     250.41ms     201.33ms     250.45ms      1
  nanosleep               2       2.000s       1.000s     805.31ms       1.000s      1
  read                    4     659.8µs     164.9µs     196.6µs     212.3µs      1
```

## Install

```bash
# deps (Ubuntu/Debian)
sudo apt install clang llvm libbpf-dev libelf-dev zlib1g-dev cmake g++-13 linux-tools-common

# build
./scripts/gen_vmlinux.sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# verify
sudo ./build/kscope doctor
```

## Usage

```bash
# live dashboards
sudo kscope top                              # latency ranking
sudo kscope top-offcpu                       # off-CPU ranking
sudo kscope top-syscalls                     # syscall stats

# targeted tracing
sudo kscope trace --pid 1234 --min-latency-us 500
sudo kscope hot-stacks --pid 1234 --user-stacks

# record & replay (replay needs no root)
sudo kscope record --duration 30 -o session.ksr
kscope replay session.ksr

# export
sudo kscope export --json > report.json
sudo kscope export --format prometheus
sudo kscope export --format otel

# interactive TUI (keys 1-6, q to quit)
sudo kscope tui
```

<details>
<summary><b>All CLI flags</b></summary>

| Flag | Default | Description |
|------|---------|-------------|
| `--pid <n>` | all | Filter by PID |
| `--comm <name>` | all | Filter by process name |
| `--duration <sec>` | ∞ | Collection duration |
| `--min-latency-us <n>` | 100 | Slow syscall threshold |
| `--min-offcpu-us <n>` | 1000 | Off-CPU threshold |
| `--syscall <name\|id>` | all | Filter to one syscall |
| `--user-stacks` | off | Capture user-space stacks |
| `--kernel-stacks` | off | Capture kernel stacks |
| `--stack-sampling <n>` | 100 | Stack sampling % (0-100) |
| `--window <sec>` | 5 | Aggregation window |
| `--top <n>` | 20 | Entries to display |
| `--json` | off | JSON output |
| `--record <file>` | — | Record to .ksr |
| `--replay <file>` | — | Replay from .ksr |

</details>

## How it works

```
┌──────────────────── Kernel ────────────────────┐
│                                                │
│  sys_enter ──┐                                 │
│  sys_exit  ──┼── kscope.bpf.c ── ring buffer ──┼──┐
│  sched_switch┤      │                          │  │
│  proc_exec ──┤   BPF maps                      │  │
│  proc_exit ──┘   (state machines)              │  │
│                                                │  │
└────────────────────────────────────────────────┘  │
                                                    │
┌───────────────────── User ────────────────────────┤
│                                                   │
│  Collector ─→ Classifier ─→ Aggregator            │
│       │            │             │                 │
│    Recorder     Symbolizer    CLI / TUI            │
│    (.ksr)      (ELF+proc)   Export (JSON/OTel)     │
│                                                    │
└────────────────────────────────────────────────────┘
```

**BPF layer** — 5 tracepoints, CO-RE for cross-kernel portability, ring buffer for ordered zero-copy delivery.

**Classification** — each slow event is tagged:

| Category | Trigger |
|----------|---------|
| `off_cpu` | sched_switch off-CPU above threshold |
| `io_wait` | slow read/write/fsync/connect/accept/recv/send |
| `lock_wait` | slow futex |
| `syscall_blocked` | slow syscall outside I/O and lock groups |
| `unknown_wait` | unclassified slow event |

**Aggregation** — per-process, per-thread, per-syscall, per-stack, with log₂ histograms (P50/P95/P99) and rolling time windows.

## Performance

Measured on x86_64, kernel 6.6:

| Scenario | Events/sec | Lost | RSS |
|----------|-----------|------|-----|
| idle | 146 | 0 | 37 MB |
| syscall-heavy | 162 | 0 | 37 MB |
| I/O-heavy | 142 | 0 | 37 MB |
| thread contention | 2,392 | 0 | 36 MB |
| short-lived procs | 5,284 | 0 | 36 MB |

Target: **< 3% overhead**, **< 150 MB RSS**, **< 1s startup**.

## Testing

```bash
# 44 unit tests
cd build && ctest

# 22 integration tests (root)
sudo bash test/integration/run_all.sh ./build

# 5 benchmark scenarios
sudo bash scripts/bench.sh ./build 10
```

Four workloads included in `examples/`: CPU stress, I/O stress, futex contention, network echo.

## Configuration

`kscope.toml`:

```toml
[thresholds]
min_latency_us = 100
min_offcpu_us  = 1000

[stacks]
user         = true
kernel       = false
sampling_pct = 50

[export]
default_format = "json"
```

## Docs

| | |
|---|---|
| [architecture.md](docs/architecture.md) | System design and data flow |
| [event-model.md](docs/event-model.md) | Event types and BPF maps |
| [performance.md](docs/performance.md) | Overhead targets and benchmarks |
| [compatibility.md](docs/compatibility.md) | Kernel and distro matrix |
| [troubleshooting.md](docs/troubleshooting.md) | Common issues |
| [demo.md](docs/demo.md) | Walkthrough with example workloads |

## Requirements

- Linux 6.1+ with BTF
- x86_64 (primary), aarch64 (secondary)
- root or CAP_BPF + CAP_PERFMON

| | Ubuntu/Debian | Fedora |
|---|---|---|
| libbpf | `libbpf-dev` | `libbpf-devel` |
| libelf | `libelf-dev` | `elfutils-libelf-devel` |
| zlib | `zlib1g-dev` | `zlib-devel` |
| clang | `clang` | `clang` |
| bpftool | `linux-tools-common` | `bpftool` |

## License

MIT
