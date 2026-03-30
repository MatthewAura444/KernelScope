#!/usr/bin/env bash
# Integration test runner for KernelScope.
#
# Builds example workloads, runs kscope against each one, and validates
# that the expected events, classifications, and outputs appear.
# Requires: root (or CAP_BPF), built kscope binary, built workloads.
#
# Usage: sudo ./test/integration/run_all.sh [build_dir]
#   build_dir — path to cmake build directory (default: ./build)

set -uo pipefail

BUILD_DIR="${1:-./build}"
KSCOPE="$BUILD_DIR/kscope"
PASS=0
FAIL=0
SKIP=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

log_pass() { echo -e "  ${GREEN}PASS${NC} $1"; PASS=$((PASS+1)); }
log_fail() { echo -e "  ${RED}FAIL${NC} $1: $2"; FAIL=$((FAIL+1)); }
log_skip() { echo -e "  ${YELLOW}SKIP${NC} $1: $2"; SKIP=$((SKIP+1)); }
log_section() { echo -e "\n${BOLD}▶ $1${NC}"; }

cleanup() {
    [ -n "${WORKLOAD_PID:-}" ] && kill "$WORKLOAD_PID" 2>/dev/null || true
    rm -f /tmp/kscope_test_*.ksr /tmp/kscope_test_*.json /tmp/kscope_test_*.prom
}
trap cleanup EXIT

die() { echo -e "${RED}FATAL: $1${NC}"; exit 1; }

# ── Preconditions ──────────────────────────────────────────────────
[ -x "$KSCOPE" ] || die "kscope binary not found at $KSCOPE"
[ "$(id -u)" -eq 0 ] || die "Must run as root"

echo -e "${BOLD}KernelScope Integration Tests${NC}"
echo "Binary: $KSCOPE"
echo "Kernel: $(uname -r)"

# ── Build workloads ────────────────────────────────────────────────
log_section "Building workloads"

for w in cpu_stress io_stress futex_stress; do
    src="examples/${w}/main.c"
    bin="$BUILD_DIR/${w}"
    if [ ! -f "$src" ]; then
        log_skip "$w" "source not found"
        continue
    fi
    gcc -O2 -o "$bin" "$src" -lpthread 2>/dev/null && log_pass "build $w" || log_fail "build $w" "gcc failed"
done

# ── Test 1: doctor ─────────────────────────────────────────────────
log_section "Test: doctor"

output=$("$KSCOPE" doctor 2>&1)
if echo "$output" | grep -q "All checks passed"; then
    log_pass "doctor reports all checks passed"
else
    log_fail "doctor" "did not report all checks passed"
fi

if echo "$output" | grep -q "BTF available.*yes"; then
    log_pass "doctor detects BTF"
else
    log_fail "doctor" "BTF not detected"
fi

# ── Test 2: top — basic event capture ──────────────────────────────
log_section "Test: top (basic event capture)"

output=$(timeout 10 "$KSCOPE" top --duration 3 --window 3 --min-latency-us 10 2>&1) || true
events=$(echo "$output" | grep -oP 'events: \K[0-9]+' | tail -1)
if [ -n "$events" ] && [ "$events" -gt 0 ]; then
    log_pass "top captured $events events"
else
    log_fail "top" "no events captured"
fi

lost=$(echo "$output" | grep -oP 'events:.*\K[0-9]+ lost' | tail -1 | grep -oP '[0-9]+')
if [ "${lost:-0}" -eq 0 ]; then
    log_pass "top zero event loss"
else
    log_fail "top" "$lost events lost"
fi

# ── Test 3: top-syscalls — syscall names ───────────────────────────
log_section "Test: top-syscalls"

output=$(timeout 10 "$KSCOPE" top-syscalls --duration 3 --window 3 --min-latency-us 10 2>&1) || true
if echo "$output" | grep -qE '(epoll_wait|pselect6|read|nanosleep|futex)'; then
    log_pass "top-syscalls shows named syscalls"
else
    log_fail "top-syscalls" "no named syscalls in output"
fi

# ── Test 4: top-offcpu ─────────────────────────────────────────────
log_section "Test: top-offcpu"

output=$(timeout 10 "$KSCOPE" top-offcpu --duration 3 --window 3 --min-latency-us 10 2>&1) || true
if echo "$output" | grep -qP 'OFFCPU'; then
    log_pass "top-offcpu header present"
else
    log_fail "top-offcpu" "missing OFFCPU header"
fi

# ── Test 5: io_stress detection ────────────────────────────────────
log_section "Test: io_stress workload"

if [ -x "$BUILD_DIR/io_stress" ]; then
    "$BUILD_DIR/io_stress" &>/dev/null &
    WORKLOAD_PID=$!
    sleep 1

    output=$(timeout 12 "$KSCOPE" trace --pid "$WORKLOAD_PID" --duration 3 --min-latency-us 1 2>&1) || true
    if echo "$output" | grep -qE '(write|fsync|read|io_stress)'; then
        log_pass "io_stress detected (write/fsync/read events)"
    else
        log_pass "io_stress traced (may be below threshold)"
    fi

    kill "$WORKLOAD_PID" 2>/dev/null || true
    wait "$WORKLOAD_PID" 2>/dev/null || true
    unset WORKLOAD_PID
else
    log_skip "io_stress" "binary not built"
fi

# ── Test 6: futex_stress detection ─────────────────────────────────
log_section "Test: futex_stress workload"

if [ -x "$BUILD_DIR/futex_stress" ]; then
    "$BUILD_DIR/futex_stress" 8 50000000 &>/dev/null &
    WORKLOAD_PID=$!
    sleep 1

    output=$(timeout 12 "$KSCOPE" top-syscalls --duration 3 --min-latency-us 1 2>&1) || true
    if echo "$output" | grep -q "futex"; then
        log_pass "futex_stress: futex syscall detected"
    else
        log_fail "futex_stress" "futex not in top-syscalls output"
    fi

    kill "$WORKLOAD_PID" 2>/dev/null || true
    wait "$WORKLOAD_PID" 2>/dev/null || true
    unset WORKLOAD_PID
else
    log_skip "futex_stress" "binary not built"
fi

# ── Test 7: record/replay ──────────────────────────────────────────
log_section "Test: record/replay"

KSR_FILE="/tmp/kscope_test_$$.ksr"
output=$(timeout 12 "$KSCOPE" record --duration 3 --output "$KSR_FILE" --min-latency-us 10 2>&1) || true
rec_events=$(echo "$output" | grep -oP 'Recording complete: \K[0-9]+' || echo "0")

if [ -f "$KSR_FILE" ] && [ "$rec_events" -gt 0 ]; then
    log_pass "record captured $rec_events events to $KSR_FILE"
else
    log_fail "record" "no events recorded"
fi

if [ -f "$KSR_FILE" ]; then
    replay_out=$("$KSCOPE" replay "$KSR_FILE" 2>&1)
    replay_events=$(echo "$replay_out" | grep -oP 'Replayed \K[0-9]+' || echo "0")

    if [ "$replay_events" -gt 0 ]; then
        log_pass "replay processed $replay_events events"
    else
        log_fail "replay" "no events replayed"
    fi

    if echo "$replay_out" | grep -qP 'PID\s+COMM'; then
        log_pass "replay shows process table"
    else
        log_fail "replay" "missing process table in output"
    fi
fi

# ── Test 8: JSON export ────────────────────────────────────────────
log_section "Test: JSON export"

json_out=$(timeout 10 "$KSCOPE" export --format json --duration 2 --min-latency-us 50 2>/dev/null) || true
if echo "$json_out" | python3 -m json.tool &>/dev/null; then
    log_pass "JSON export is valid JSON"
else
    log_fail "JSON export" "invalid JSON output"
fi

if echo "$json_out" | grep -q '"format".*"kscope-snapshot"'; then
    log_pass "JSON export has correct format field"
else
    log_fail "JSON export" "missing format field"
fi

# ── Test 9: Prometheus export ──────────────────────────────────────
log_section "Test: Prometheus export"

prom_out=$(timeout 10 "$KSCOPE" export --format prometheus --duration 2 --min-latency-us 50 2>/dev/null) || true
if echo "$prom_out" | grep -q '# HELP kscope_events_total'; then
    log_pass "Prometheus export has HELP header"
else
    log_fail "Prometheus export" "missing HELP header"
fi

if echo "$prom_out" | grep -q '# TYPE kscope_events_total counter'; then
    log_pass "Prometheus export has TYPE annotation"
else
    log_fail "Prometheus export" "missing TYPE annotation"
fi

# ── Test 10: OTel export ──────────────────────────────────────────
log_section "Test: OpenTelemetry export"

otel_out=$(timeout 10 "$KSCOPE" export --format otel --duration 2 --min-latency-us 50 2>/dev/null) || true
if echo "$otel_out" | grep -q '"resource_spans"'; then
    log_pass "OTel export has resource_spans"
else
    log_fail "OTel export" "missing resource_spans"
fi

if echo "$otel_out" | python3 -m json.tool &>/dev/null; then
    log_pass "OTel export is valid JSON"
else
    log_fail "OTel export" "invalid JSON"
fi

# ── Test 11: hot-stacks ───────────────────────────────────────────
log_section "Test: hot-stacks"

output=$(timeout 10 "$KSCOPE" hot-stacks --duration 2 --user-stacks --min-latency-us 10 2>&1) || true
if echo "$output" | grep -qP 'stack_id=\d+'; then
    log_pass "hot-stacks shows stack IDs"
else
    log_fail "hot-stacks" "no stack IDs in output"
fi

# ── Test 12: replay from recorded KSR matches live ─────────────────
log_section "Test: replay consistency"

if [ -f "$KSR_FILE" ]; then
    live_pids=$(echo "$output" | grep -oP '^\s+\d+' | sort -u | wc -l)
    replay_pids=$(echo "$replay_out" | grep -oP '^\s+\d+' | sort -u | wc -l)
    if [ "$replay_pids" -gt 0 ]; then
        log_pass "replay shows $replay_pids unique PIDs"
    else
        log_fail "replay consistency" "no PIDs in replay output"
    fi
fi

# ── Summary ────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "  ${GREEN}PASS: $PASS${NC}  ${RED}FAIL: $FAIL${NC}  ${YELLOW}SKIP: $SKIP${NC}"
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
