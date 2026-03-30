# KernelScope Compatibility

## Kernel Requirements

| Requirement     | Minimum   | Recommended |
|-----------------|-----------|-------------|
| Kernel version  | 6.1       | 6.6+        |
| BTF support     | Required  | —           |
| BPF ring buffer | Required  | —           |
| BPF CO-RE       | Required  | —           |

### Checking BTF Availability

```bash
ls -la /sys/kernel/btf/vmlinux
```

If this file exists, BTF is available.  Most modern distributions
(Ubuntu 22.04+, Fedora 37+, Debian 12+, Arch Linux) ship BTF-enabled
kernels.

### Checking Kernel Version

```bash
uname -r
# Must be >= 6.1
```

## Architecture Support

| Architecture | Status    | Notes                        |
|--------------|-----------|------------------------------|
| x86_64       | Primary   | Fully tested                 |
| aarch64      | Secondary | Syscall numbers differ       |

**Note:** Syscall ID-to-name mappings in `event_decoder.cpp` are
x86_64-specific.  On aarch64, the numbers differ and the display
will fall back to `"syscall"` for unrecognised IDs.

## Distribution Matrix

| Distribution     | Kernel  | BTF  | Status    |
|------------------|---------|------|-----------|
| Ubuntu 24.04 LTS | 6.8     | ✓    | Tested    |
| Ubuntu 22.04 LTS | 5.15 → 6.2 HWE | ✓ | HWE kernel required |
| Fedora 40        | 6.8     | ✓    | Tested    |
| Debian 12        | 6.1     | ✓    | Minimum   |
| Arch Linux       | Rolling | ✓    | Tested    |
| RHEL 9           | 5.14    | ✓    | **Below minimum** — use newer kernel |
| Amazon Linux 2023| 6.1     | ✓    | Minimum   |

## Privilege Requirements

KernelScope attaches BPF programs to kernel tracepoints and reads
from BPF maps.  This requires elevated privileges:

| Method            | Required Capabilities              |
|-------------------|------------------------------------|
| Run as root       | All capabilities (works always)    |
| `CAP_BPF` + `CAP_PERFMON` | Sufficient for modern kernels |
| `CAP_SYS_ADMIN`  | Legacy fallback                    |

### Setting Capabilities

```bash
sudo setcap cap_bpf,cap_perfmon=ep ./kscope
```

## Build Dependencies

| Package           | Debian/Ubuntu           | Fedora/RHEL            |
|-------------------|-------------------------|------------------------|
| libbpf            | `libbpf-dev`            | `libbpf-devel`         |
| libelf            | `libelf-dev`            | `elfutils-libelf-devel`|
| zlib              | `zlib1g-dev`            | `zlib-devel`           |
| clang/LLVM        | `clang`                 | `clang`                |
| bpftool           | `linux-tools-common`    | `bpftool`              |
| CMake ≥ 3.22      | `cmake`                 | `cmake`                |
| C++23 compiler    | `g++-13` or `clang-17`  | `gcc-toolset-13`       |

## `kscope doctor` Output

The `doctor` command checks all prerequisites:

```
$ sudo kscope doctor
KernelScope v1.0.0 — System Check
  Kernel:   6.8.0-45-generic ✓ (>= 6.1 required)
  BTF:      /sys/kernel/btf/vmlinux found ✓
  libbpf:   1.3.0 ✓
  Arch:     x86_64 ✓
  Caps:     CAP_SYS_ADMIN ✓
  Status:   All checks passed ✓
```
