#!/usr/bin/env bash
# gen_skel.sh — Manual BPF skeleton generation fallback.
#
# Use this if the CMake build doesn't generate the skeleton correctly.
# Compiles the BPF program and generates the skeleton header.
#
# Usage: ./scripts/gen_skel.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BPF_DIR="$PROJECT_DIR/bpf"
BUILD_DIR="$PROJECT_DIR/build"

mkdir -p "$BUILD_DIR"

ARCH=$(uname -m)
case "$ARCH" in
    x86_64)  TARGET_ARCH="x86"    ;;
    aarch64) TARGET_ARCH="arm64"  ;;
    *)       TARGET_ARCH="x86"    ;;
esac

echo "Compiling BPF program (arch=$TARGET_ARCH)..."
clang -g -O2 \
    -target bpf \
    -D__TARGET_ARCH_${TARGET_ARCH} \
    -I "$BPF_DIR" \
    -c "$BPF_DIR/kscope.bpf.c" \
    -o "$BUILD_DIR/kscope.bpf.o"

echo "Generating skeleton..."
bpftool gen skeleton "$BUILD_DIR/kscope.bpf.o" > "$BUILD_DIR/kscope.skel.h"

echo "Done. Skeleton at: $BUILD_DIR/kscope.skel.h"
