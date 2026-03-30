# KernelScope Troubleshooting

## Common Issues

### 1. "Failed to open BPF object"

**Cause:** libbpf cannot find or parse the embedded skeleton.

**Fix:**
- Verify the build completed without errors.
- Ensure `bpftool gen skeleton` ran during the build.
- Check that clang can target BPF: `clang -target bpf -c /dev/null -o /dev/null`.

### 2. "BTF not available"

**Cause:** The running kernel was not compiled with `CONFIG_DEBUG_INFO_BTF=y`.

**Fix:**
- Check: `ls /sys/kernel/btf/vmlinux`
- On Ubuntu: install the HWE kernel (`linux-generic-hwe-22.04`).
- On Fedora/Arch: BTF is enabled by default.
- On custom kernels: enable `CONFIG_DEBUG_INFO_BTF=y` in `.config`.

### 3. "Permission denied" or "Operation not permitted"

**Cause:** Insufficient privileges to load BPF programs.

**Fix:**
```bash
# Option 1: run as root
sudo kscope top

# Option 2: set capabilities
sudo setcap cap_bpf,cap_perfmon=ep ./kscope
```

### 4. "Ring buffer overflow — events lost"

**Cause:** Events are generated faster than user space can consume them.

**Fix:**
- Increase ring buffer size in `kscope.toml`:
  ```toml
  [ringbuf]
  size_mb = 64
  ```
- Raise the minimum latency threshold:
  ```bash
  kscope top --min-latency-us 1000
  ```
- Disable stack capture if not needed:
  ```bash
  kscope top --stack-sampling 0
  ```

### 5. Symbols show as raw hex addresses (`0x7f...`)

**Cause:** Symbolisation failed — binary not found, stripped, or
`/proc/<pid>/maps` unavailable.

**Fix:**
- Ensure the target process is still running (maps are read lazily).
- Install debug symbols for the target binary.
- For kernel stacks, ensure `/proc/kallsyms` is readable:
  ```bash
  echo 0 | sudo tee /proc/sys/kernel/kptr_restrict
  ```

### 6. "Unsupported kernel version"

**Cause:** Kernel is older than 6.1.

**Fix:** Upgrade to kernel 6.1 or newer.  See `docs/compatibility.md`
for the distribution matrix.

### 7. High overhead (> 5%)

**Cause:** Too many events captured, stack capture too frequent.

**Fix:**
- Increase `--min-latency-us` (e.g., from 100 to 500).
- Reduce `--stack-sampling` (e.g., from 100 to 10).
- Disable kernel stacks if not needed.
- Filter to a specific PID: `kscope top --pid <N>`.

### 8. `.ksr` replay shows different numbers than live

**Cause:** Expected behaviour — replay processes all events without
real-time pacing, and timer-based window boundaries may differ.

**Context:** Event timestamps are preserved, but aggregation windows
are computed from event timestamps during replay rather than wall
clock, which can shift window boundaries slightly.

### 9. Build fails with "cannot find -lbpf"

**Cause:** libbpf is not installed or not found by CMake.

**Fix:**
```bash
# Debian/Ubuntu
sudo apt install libbpf-dev

# Fedora
sudo dnf install libbpf-devel

# Or build libbpf from source:
git clone https://github.com/libbpf/libbpf
cd libbpf/src && make && sudo make install
```

### 10. `sched_switch` events not captured

**Cause:** The `sched/sched_switch` tracepoint may require
`CAP_SYS_ADMIN` on some kernels even when `CAP_BPF` is present.

**Fix:** Run as root or add `CAP_SYS_ADMIN`.

## Diagnostics

Run `kscope doctor` to check all prerequisites before profiling.

The TUI's Diagnostics screen (press `6`) shows live counters:
- Events received / lost
- Active syscall map entries
- Scheduler state map entries

## Getting Help

If you encounter an issue not covered here:

1. Run `kscope doctor` and include the output.
2. Check kernel version: `uname -r`.
3. Check BPF support: `bpftool feature`.
4. Open an issue with the above information.
