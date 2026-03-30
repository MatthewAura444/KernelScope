#!/usr/bin/env bash
# gen_vmlinux.sh — Generate vmlinux.h from the running kernel's BTF.
#
# This script extracts the BTF-based vmlinux.h header that BPF programs
# need for CO-RE.  Run this on the target machine before building if
# a pre-generated vmlinux.h is not available.
#
# Usage: ./scripts/gen_vmlinux.sh [output_path]
#
# Requires: bpftool, /sys/kernel/btf/vmlinux

set -euo pipefail

OUTPUT="${1:-bpf/vmlinux.h}"

if [ ! -f /sys/kernel/btf/vmlinux ]; then
    echo "ERROR: /sys/kernel/btf/vmlinux not found. Is BTF enabled?" >&2
    exit 1
fi

echo "Generating $OUTPUT from kernel BTF..."
bpftool btf dump file /sys/kernel/btf/vmlinux format c > "$OUTPUT"
echo "Done. $(wc -l < "$OUTPUT") lines generated."
