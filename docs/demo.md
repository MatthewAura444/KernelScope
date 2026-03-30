# KernelScope Demo Walkthrough

## Prerequisites

1. Linux 6.1+ with BTF enabled
2. Built `kscope` binary (see README.md)
3. Root access or `CAP_BPF` + `CAP_PERFMON`

## Demo 1: Detect I/O Latency

Build and launch the I/O stress workload:

```bash
gcc examples/io_stress/main.c -o io_stress
./io_stress 500 4096 &
IO_PID=$!
```

Profile with KernelScope:

```bash
sudo ./build/kscope trace --pid $IO_PID --min-latency-us 50
```

Expected output — streams of `io_wait` classified events showing
`read`, `write`, and `fsync` syscalls with their durations:

```
[1234.567] pid=9876 tid=9876 io_stress       write          dur=1.23ms       class=io_wait
[1234.568] pid=9876 tid=9876 io_stress       fsync          dur=5.67ms       class=io_wait
[1234.569] pid=9876 tid=9876 io_stress       read           dur=0.89ms       class=io_wait
```

Aggregate view:

```bash
sudo ./build/kscope top-syscalls --pid $IO_PID --window 5
```

Shows per-syscall statistics with count, total, avg, P95, and max.

## Demo 2: Detect Lock Contention

Build and launch the futex stress workload:

```bash
gcc examples/futex_stress/main.c -o futex_stress -lpthread
./futex_stress 16 50000 &
FX_PID=$!
```

Profile:

```bash
sudo ./build/kscope trace --pid $FX_PID --min-latency-us 10
```

Expected — `lock_wait` events from futex syscalls:

```
[2345.678] pid=1111 tid=1115 futex_stress    futex          dur=234.56us     class=lock_wait
```

Top off-CPU view shows threads ranked by waiting time:

```bash
sudo ./build/kscope top-offcpu --pid $FX_PID
```

## Demo 3: Record and Replay

Record a 10-second session:

```bash
sudo ./build/kscope record --pid $IO_PID --duration 10 --output demo.ksr
```

Replay without root (offline analysis):

```bash
./build/kscope replay demo.ksr
```

Export to JSON:

```bash
./build/kscope replay demo.ksr --json > demo.json
```

## Demo 4: Stack Traces

Show the hottest stack traces for slow events:

```bash
sudo ./build/kscope hot-stacks --pid $IO_PID --user-stacks --min-latency-us 100
```

Expected — top stacks with symbolised frames:

```
#1 stack=42 total=12.34ms count=15 type=syscall_slow
    #0 __GI___libc_write (libc.so.6+0x114a20)
    #1 main (io_stress+0x1234)
    #2 __libc_start_main (libc.so.6+0x29d90)
```

## Demo 5: Export Formats

JSON snapshot:

```bash
sudo ./build/kscope export --pid $IO_PID --duration 5 --json
```

The JSON output includes per-process stats (with P50/P95/P99), per-syscall
stats, and diagnostics.

## Demo 6: TUI Mode

Launch the full-screen terminal UI:

```bash
sudo ./build/kscope tui
```

Navigate with keys `1`-`6` to switch between:
1. **Overview** — summary with top processes and syscalls
2. **Processes** — sorted by total latency
3. **Threads** — per-thread breakdown
4. **Syscalls** — per-syscall statistics
5. **Hot Stacks** — symbolised stack traces
6. **Diagnostics** — event counts, map pressure, ring buffer status

Press `q` to quit.
