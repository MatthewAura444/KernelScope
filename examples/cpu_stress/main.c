/*
 * cpu_stress — CPU-intensive workload for KernelScope testing.
 *
 * Alternates between busy-loop computation (visible as on_cpu time)
 * and short sleeps (visible as off_cpu time).  Useful for verifying
 * that KernelScope correctly distinguishes CPU work from scheduler
 * sleep and that sched_switch tracking works.
 *
 * Usage: cpu_stress [iterations] [sleep_us]
 *   iterations  — number of busy/sleep cycles (default: 100)
 *   sleep_us    — microseconds to sleep per cycle (default: 1000)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile double sink;

static void busy_work(int n) {
    double x = 1.0;
    for (int i = 0; i < n; i++) {
        x = x * 1.0001 + 0.0001;
    }
    sink = x;
}

int main(int argc, char** argv) {
    int iterations = argc > 1 ? atoi(argv[1]) : 100;
    int sleep_us   = argc > 2 ? atoi(argv[2]) : 1000;

    printf("cpu_stress: %d iterations, %d us sleep\n", iterations, sleep_us);
    printf("PID: %d\n", getpid());

    struct timespec ts;
    ts.tv_sec  = sleep_us / 1000000;
    ts.tv_nsec = (sleep_us % 1000000) * 1000L;

    for (int i = 0; i < iterations; i++) {
        busy_work(100000);
        nanosleep(&ts, NULL);
    }

    printf("cpu_stress: done\n");
    return 0;
}
