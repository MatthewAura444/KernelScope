/*
 * io_stress — I/O-intensive workload for KernelScope testing.
 *
 * Creates a temporary file and performs repeated write/fsync/read
 * operations.  Each operation is a slow syscall that should appear
 * as io_wait events in KernelScope, allowing verification of I/O
 * classification and syscall latency measurement.
 *
 * Usage: io_stress [iterations] [block_size]
 *   iterations  — number of write/read cycles (default: 200)
 *   block_size  — bytes per write (default: 4096)
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int iterations  = argc > 1 ? atoi(argv[1]) : 200;
    int block_size  = argc > 2 ? atoi(argv[2]) : 4096;

    printf("io_stress: %d iterations, %d byte blocks\n", iterations, block_size);
    printf("PID: %d\n", getpid());

    char path[] = "/tmp/kscope_io_stress_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }
    unlink(path);

    char* buf = malloc(block_size);
    if (!buf) {
        perror("malloc");
        close(fd);
        return 1;
    }
    memset(buf, 'A', block_size);

    for (int i = 0; i < iterations; i++) {
        if (write(fd, buf, block_size) != block_size) {
            perror("write");
            break;
        }

        if (i % 10 == 0) {
            fsync(fd);
        }

        if (lseek(fd, 0, SEEK_SET) < 0) {
            perror("lseek");
            break;
        }

        if (read(fd, buf, block_size) != block_size) {
            perror("read");
            break;
        }

        if (lseek(fd, 0, SEEK_END) < 0) {
            perror("lseek end");
            break;
        }
    }

    free(buf);
    close(fd);
    printf("io_stress: done\n");
    return 0;
}
