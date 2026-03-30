/*
 * futex_stress — Lock contention workload for KernelScope testing.
 *
 * Spawns multiple threads competing for a single mutex, creating
 * futex(FUTEX_WAIT) syscalls that should be classified as lock_wait
 * events.  Critical sections are deliberately short to maximise
 * contention.
 *
 * Usage: futex_stress [threads] [iterations]
 *   threads    — number of contending threads (default: 8)
 *   iterations — lock/unlock cycles per thread (default: 10000)
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static volatile long counter;

struct args {
    int id;
    int iterations;
};

static void* worker(void* arg) {
    struct args* a = (struct args*)arg;

    for (int i = 0; i < a->iterations; i++) {
        pthread_mutex_lock(&mtx);
        counter++;
        pthread_mutex_unlock(&mtx);
    }

    return NULL;
}

int main(int argc, char** argv) {
    int nthreads   = argc > 1 ? atoi(argv[1]) : 8;
    int iterations = argc > 2 ? atoi(argv[2]) : 10000;

    printf("futex_stress: %d threads, %d iterations\n", nthreads, iterations);
    printf("PID: %d\n", getpid());

    pthread_t* threads = malloc(sizeof(pthread_t) * nthreads);
    struct args* targs = malloc(sizeof(struct args) * nthreads);

    if (!threads || !targs) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < nthreads; i++) {
        targs[i].id = i;
        targs[i].iterations = iterations;
        if (pthread_create(&threads[i], NULL, worker, &targs[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("futex_stress: counter = %ld (expected %ld)\n",
           counter, (long)nthreads * iterations);

    free(threads);
    free(targs);
    printf("futex_stress: done\n");
    return 0;
}
