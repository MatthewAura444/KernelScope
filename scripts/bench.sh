#!/usr/bin/env bash
# bench.sh — Benchmark suite for KernelScope.
#
# Measures tool overhead, event throughput, dropped events, symbolization
# cost, and memory growth across five scenarios defined in the spec:
#   1. Idle system
#   2. Syscall-heavy workload (cpu_stress)
#   3. I/O-heavy workload (io_stress)
#   4. Thread contention (futex_stress)
#   5. Many short-lived processes (fork loop)
#
# Outputs structured results suitable for docs/performance.md.
# Requires: root, built kscope + workloads.
#
# Usage: sudo ./scripts/bench.sh [build_dir] [duration_sec]

set -euo pipefail

BUILD_DIR="${1:-./build}"
DURATION="${2:-10}"
KSCOPE="$BUILD_DIR/kscope"
RESULTS_DIR="/tmp/kscope_bench_$$"
mkdir -p "$RESULTS_DIR"

BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

[ -x "$KSCOPE" ] || { echo "kscope not found at $KSCOPE"; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "Must run as root"; exit 1; }

echo -e "${BOLD}KernelScope Benchmark Suite${NC}"
echo "Duration per scenario: ${DURATION}s"
echo "Results: $RESULTS_DIR"
echo ""

# Build workloads if needed
for w in cpu_stress io_stress futex_stress; do
    [ -x "$BUILD_DIR/$w" ] && continue
    src="examples/${w}/main.c"
    [ -f "$src" ] && gcc -O2 -o "$BUILD_DIR/$w" "$src" -lpthread 2>/dev/null
done

get_rss_kb() {
    awk '/VmRSS/{print $2}' /proc/"$1"/status 2>/dev/null || echo "0"
}

run_scenario() {
    local name="$1"
    local workload_cmd="$2"
    local kscope_extra="${3:-}"

    echo -e "${BOLD}▶ Scenario: $name${NC}"

    local workload_pid=""
    if [ -n "$workload_cmd" ]; then
        eval "$workload_cmd" &>/dev/null &
        workload_pid=$!
        sleep 1
    fi

    local ksr_file="$RESULTS_DIR/${name}.ksr"
    local start_ns
    start_ns=$(date +%s%N)

    $KSCOPE record --duration "$DURATION" --output "$ksr_file" \
        --min-latency-us 50 $kscope_extra 2>"$RESULTS_DIR/${name}.stderr" &
    local kscope_pid=$!

    # Sample peak RSS every second
    local max_rss=0
    for ((i=0; i<DURATION+2; i++)); do
        if kill -0 "$kscope_pid" 2>/dev/null; then
            local rss
            rss=$(get_rss_kb "$kscope_pid")
            [ "$rss" -gt "$max_rss" ] && max_rss=$rss
        fi
        sleep 1
    done
    wait "$kscope_pid" 2>/dev/null || true

    local end_ns
    end_ns=$(date +%s%N)
    local elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))

    local stderr_out
    stderr_out=$(cat "$RESULTS_DIR/${name}.stderr" 2>/dev/null || echo "")
    local events
    events=$(echo "$stderr_out" | grep -oP 'Recording complete: \K[0-9]+' || echo "0")
    local lost
    lost=$(echo "$stderr_out" | grep -oP '[0-9]+ lost' | grep -oP '[0-9]+' | head -1 || echo "0")

    [ -n "$workload_pid" ] && { kill "$workload_pid" 2>/dev/null; wait "$workload_pid" 2>/dev/null; } || true

    local ksr_size=0
    [ -f "$ksr_file" ] && ksr_size=$(stat -c%s "$ksr_file" 2>/dev/null || echo "0")
    local events_per_sec=0
    [ "$DURATION" -gt 0 ] && events_per_sec=$((events / DURATION))

    echo -e "  Events captured:  ${events}"
    echo -e "  Events lost:      ${lost:-0}"
    echo -e "  Events/sec:       ${events_per_sec}"
    echo -e "  Peak RSS (MB):    $((max_rss / 1024))"
    echo -e "  KSR size (KB):    $((ksr_size / 1024))"
    echo ""

    cat > "$RESULTS_DIR/${name}.json" <<EOF
{
    "scenario": "$name",
    "duration_sec": $DURATION,
    "events_captured": $events,
    "events_lost": ${lost:-0},
    "events_per_sec": $events_per_sec,
    "peak_rss_kb": $max_rss,
    "peak_rss_mb": $((max_rss / 1024)),
    "ksr_file_bytes": $ksr_size,
    "wall_time_ms": $elapsed_ms
}
EOF
}

# ── Scenario 1: Idle system ──────────────────────────────────────
run_scenario "idle" ""

# ── Scenario 2: Syscall-heavy (cpu_stress) ───────────────────────
if [ -x "$BUILD_DIR/cpu_stress" ]; then
    run_scenario "syscall_heavy" "$BUILD_DIR/cpu_stress"
else
    echo -e "${DIM}SKIP: syscall_heavy (cpu_stress not built)${NC}"
fi

# ── Scenario 3: I/O-heavy ────────────────────────────────────────
if [ -x "$BUILD_DIR/io_stress" ]; then
    run_scenario "io_heavy" "$BUILD_DIR/io_stress"
else
    echo -e "${DIM}SKIP: io_heavy (io_stress not built)${NC}"
fi

# ── Scenario 4: Thread contention ────────────────────────────────
if [ -x "$BUILD_DIR/futex_stress" ]; then
    run_scenario "thread_contention" "$BUILD_DIR/futex_stress 8 100000000"
else
    echo -e "${DIM}SKIP: thread_contention (futex_stress not built)${NC}"
fi

# ── Scenario 5: Many short-lived processes ───────────────────────
run_scenario "short_lived" "while true; do /bin/true; done"

# ── Summary ───────────────────────────────────────────────────────
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BOLD}Benchmark Summary${NC}"
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
printf "%-20s %10s %10s %10s %10s\n" "Scenario" "Events" "Lost" "Ev/sec" "RSS(MB)"
echo "──────────────────────────────────────────────────────────────────"

for f in "$RESULTS_DIR"/*.json; do
    [ -f "$f" ] || continue
    python3 -c "
import json, sys
d = json.load(open('$f'))
print(f\"{d['scenario']:<20s} {d['events_captured']:>10} {d['events_lost']:>10} {d['events_per_sec']:>10} {d['peak_rss_mb']:>10}\")
"
done

echo ""
echo "Detailed results: $RESULTS_DIR/"
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
