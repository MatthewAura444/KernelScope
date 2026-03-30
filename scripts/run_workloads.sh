#!/usr/bin/env bash
# run_workloads.sh — Build and run example workloads for manual profiling.
#
# Compiles the C workloads in examples/ and runs one or all of them
# in the background so you can attach kscope for testing.
#
# Usage: ./scripts/run_workloads.sh [build_dir] [workload] [duration]
#   build_dir — cmake build directory (default: ./build)
#   workload  — cpu_stress|io_stress|futex_stress|all (default: all)
#   duration  — seconds to run (default: 30)

set -uo pipefail

BUILD_DIR="${1:-./build}"
WORKLOAD="${2:-all}"
DURATION="${3:-30}"
PIDS=()

cleanup() {
    for p in "${PIDS[@]}"; do
        kill "$p" 2>/dev/null || true
    done
}
trap cleanup EXIT

build_workload() {
    local name="$1"
    local src="examples/${name}/main.c"
    [ -f "$src" ] || return 1
    gcc -O2 -pthread -o "$BUILD_DIR/$name" "$src" 2>/dev/null
}

run_workload() {
    local name="$1"
    if ! build_workload "$name"; then
        echo "  SKIP $name (source not found)"
        return
    fi
    case "$name" in
        cpu_stress)    "$BUILD_DIR/$name" &;;
        io_stress)     "$BUILD_DIR/$name" &;;
        futex_stress)  "$BUILD_DIR/$name" 4 100000000 &;;
        *)             "$BUILD_DIR/$name" &;;
    esac
    PIDS+=($!)
    echo "  $name  PID=$!"
}

echo "KernelScope Workload Runner"
echo "Workloads: $WORKLOAD  Duration: ${DURATION}s"
echo ""

if [ "$WORKLOAD" = "all" ]; then
    for w in cpu_stress io_stress futex_stress; do
        run_workload "$w"
    done
else
    run_workload "$WORKLOAD"
fi

echo ""
echo "Running for ${DURATION}s — attach kscope now:"
echo "  sudo $BUILD_DIR/kscope top --duration $DURATION"
echo ""
sleep "$DURATION"
echo "Done."
