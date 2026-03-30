/*
 * net_stress — Network I/O workload for KernelScope testing.
 *
 * Runs a local TCP echo server and client in the same process.  The
 * client connects, sends data, reads the echo, and closes — repeated
 * over many iterations.  Produces connect/accept/send/recv/close
 * syscalls that should be classified as io_wait (network) events.
 *
 * Usage: net_stress [iterations] [message_size]
 *   iterations    — number of connect/echo/close cycles (default: 100)
 *   message_size  — bytes per message (default: 256)
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int server_port;
static int server_ready;
static pthread_mutex_t ready_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ready_cv  = PTHREAD_COND_INITIALIZER;

struct server_args {
    int iterations;
    int msg_size;
};

static void* server_thread(void* arg) {
    struct server_args* sa = (struct server_args*)arg;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return NULL; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(sock); return NULL;
    }

    socklen_t addrlen = sizeof(addr);
    getsockname(sock, (struct sockaddr*)&addr, &addrlen);

    if (listen(sock, 128) < 0) {
        perror("listen"); close(sock); return NULL;
    }

    pthread_mutex_lock(&ready_mtx);
    server_port = ntohs(addr.sin_port);
    server_ready = 1;
    pthread_cond_signal(&ready_cv);
    pthread_mutex_unlock(&ready_mtx);

    char* buf = malloc(sa->msg_size);
    for (int i = 0; i < sa->iterations; i++) {
        int client = accept(sock, NULL, NULL);
        if (client < 0) { perror("accept"); break; }

        ssize_t n = recv(client, buf, sa->msg_size, MSG_WAITALL);
        if (n > 0) {
            send(client, buf, n, 0);
        }
        close(client);
    }

    free(buf);
    close(sock);
    return NULL;
}

int main(int argc, char** argv) {
    int iterations = argc > 1 ? atoi(argv[1]) : 100;
    int msg_size   = argc > 2 ? atoi(argv[2]) : 256;

    printf("net_stress: %d iterations, %d byte messages\n", iterations, msg_size);
    printf("PID: %d\n", getpid());

    struct server_args sa = { .iterations = iterations, .msg_size = msg_size };
    pthread_t srv;
    pthread_create(&srv, NULL, server_thread, &sa);

    pthread_mutex_lock(&ready_mtx);
    while (!server_ready) pthread_cond_wait(&ready_cv, &ready_mtx);
    pthread_mutex_unlock(&ready_mtx);

    char* buf = malloc(msg_size);
    memset(buf, 'B', msg_size);

    for (int i = 0; i < iterations; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { perror("client socket"); break; }

        struct sockaddr_in addr = {0};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = htons(server_port);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect"); close(sock); break;
        }

        send(sock, buf, msg_size, 0);
        recv(sock, buf, msg_size, MSG_WAITALL);
        close(sock);
    }

    free(buf);
    pthread_join(srv, NULL);

    printf("net_stress: done\n");
    return 0;
}
