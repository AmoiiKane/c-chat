#include "common.h"
#include <limits.h>

/*
 * sock_fd is written once in main before the receive thread starts,
 * then only read by both threads — no synchronization needed.
 */
static sock_t    sock_fd   = SOCK_INVALID;
static volatile int running = 1; /* FIX: shared flag to coordinate shutdown */

/* ── Receive thread ───────────────────────────────────────── */
static THREAD_FUNC receive_handler(void *arg)
{
    (void)arg;
    char buf[BUFFER_SIZE];
    int  nbytes;

    while (running && (nbytes = (int)sock_read(sock_fd, buf, BUFFER_SIZE - 1)) > 0) {
        buf[nbytes] = '\0';
        printf("%s", buf);
        fflush(stdout);
    }

    if (running) {
        /* Server closed the connection unexpectedly */
        printf("\n[disconnected from server]\n");
        running = 0;
    }

    /* FIX: don't close sock_fd here — main thread owns the fd and will close it */

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ── Main ─────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    const char *host = (argc >= 2) ? argv[1] : "127.0.0.1";
    int         port = PORT;

    /* FIX: validate port argument */
    if (argc >= 3) {
        char *endptr;
        long p = strtol(argv[2], &endptr, 10);
        if (*endptr != '\0' || p <= 0 || p > 65535) {
            fprintf(stderr, "Invalid port: %s (must be 1-65535)\n", argv[2]);
            return 1;
        }
        port = (int)p;
    }

    struct sockaddr_in server_addr;
    thread_t recv_tid;
    char buf[BUFFER_SIZE];

    if (net_init() != 0) {
        fprintf(stderr, "Network init failed\n");
        return 1;
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == SOCK_INVALID) { perror("socket"); return 1; }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", host);
        sock_close(sock_fd);
        net_cleanup();
        return 1;
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        fprintf(stderr, "Could not connect to %s:%d\n", host, port);
        sock_close(sock_fd);
        net_cleanup();
        return 1;
    }

    printf("[c-chat] connected to %s:%d\n", host, port);
    printf("Type /quit to exit\n\n");

    /* FIX: check thread creation */
    if (thread_create(&recv_tid, receive_handler, NULL) != 0) {
        fprintf(stderr, "Failed to start receive thread\n");
        sock_close(sock_fd);
        net_cleanup();
        return 1;
    }
#ifdef _WIN32
    CloseHandle(recv_tid);
#else
    pthread_detach(recv_tid);
#endif

    while (running && fgets(buf, BUFFER_SIZE, stdin)) {
        if (strlen(buf) == 0 || strcmp(buf, "\n") == 0) continue;
        sock_write(sock_fd, buf, (int)strlen(buf));

        /* FIX: strip newline before comparing — prevents prefix match on "/quitter\n" */
        char cmd[BUFFER_SIZE];
        strncpy(cmd, buf, BUFFER_SIZE - 1);
        cmd[BUFFER_SIZE - 1] = '\0';
        size_t len = strlen(cmd);
        while (len > 0 && (cmd[len - 1] == '\r' || cmd[len - 1] == '\n'))
            cmd[--len] = '\0';
        if (strcmp(cmd, "/quit") == 0) break;
    }

    /* FIX: signal receive thread and close fd once, here in main */
    running = 0;
    sock_close(sock_fd);
    net_cleanup();
    return 0;
}
