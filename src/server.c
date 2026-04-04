#include "common.h"

/* ── Client slot ──────────────────────────────────────────── */
typedef struct {
    sock_t  fd;
    int     index;
    char    username[USERNAME_SIZE];
} client_t;

static client_t *clients[MAX_CLIENTS];
static sock_t    server_fd = SOCK_INVALID;

#ifdef _WIN32
static CRITICAL_SECTION clients_mutex;
#define mutex_lock()   EnterCriticalSection(&clients_mutex)
#define mutex_unlock() LeaveCriticalSection(&clients_mutex)
#else
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
#define mutex_lock()   pthread_mutex_lock(&clients_mutex)
#define mutex_unlock() pthread_mutex_unlock(&clients_mutex)
#endif

/* ── Helpers ──────────────────────────────────────────────── */
static void add_client(client_t *cl)
{
    mutex_lock();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) { clients[i] = cl; cl->index = i; break; }
    }
    mutex_unlock();
}

static void remove_client(int index)
{
    mutex_lock();
    clients[index] = NULL;
    mutex_unlock();
}

static void broadcast(const char *msg, int sender_index)
{
    mutex_lock();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->index != sender_index)
            sock_write(clients[i]->fd, msg, (int)strlen(msg));
    }
    mutex_unlock();
}

/* Strip all trailing \r and \n from str in-place. */
static void strip_newlines(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n'))
        str[--len] = '\0';
}

/* ── Per-client thread ────────────────────────────────────── */
static THREAD_FUNC handle_client(void *arg)
{
    client_t *cl = (client_t *)arg;
    char buf[BUFFER_SIZE];
    char msg[BUFFER_SIZE + USERNAME_SIZE + 32];
    int  nbytes;

    /* FIX: use strlen instead of hardcoded length */
    const char *prompt = "Enter username: ";
    sock_write(cl->fd, prompt, (int)strlen(prompt));

    nbytes = (int)sock_read(cl->fd, cl->username, USERNAME_SIZE - 1);
    if (nbytes <= 0) {
        /* FIX: disconnected before username — skip join broadcast, clean up silently */
        sock_close(cl->fd);
        remove_client(cl->index);
        free(cl);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    cl->username[nbytes] = '\0';  /* FIX: explicit null termination */
    strip_newlines(cl->username); /* FIX: strip all trailing \r\n, not just first */

    /* FIX: reject empty usernames instead of broadcasting "[  joined]" */
    if (strlen(cl->username) == 0) {
        const char *err = "Username cannot be empty. Disconnecting.\n";
        sock_write(cl->fd, err, (int)strlen(err));
        sock_close(cl->fd);
        remove_client(cl->index);
        free(cl);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    snprintf(msg, sizeof(msg), "[%s joined the chat]\n", cl->username);
    printf("%s", msg);
    broadcast(msg, cl->index);

    while ((nbytes = (int)sock_read(cl->fd, buf, BUFFER_SIZE - 1)) > 0) {
        buf[nbytes] = '\0';
        strip_newlines(buf);
        if (strlen(buf) == 0) continue;

        /* FIX: exact match — strcmp prevents "/quitter" from triggering quit */
        if (strcmp(buf, "/quit") == 0) break;

        snprintf(msg, sizeof(msg), "[%s]: %s\n", cl->username, buf);
        printf("%s", msg);
        broadcast(msg, cl->index);
    }

    snprintf(msg, sizeof(msg), "[%s left the chat]\n", cl->username);
    printf("%s", msg);
    broadcast(msg, cl->index);

    sock_close(cl->fd);
    remove_client(cl->index);
    free(cl);

#ifdef _WIN32
    return 0;
#else
    return NULL;  /* FIX: removed pthread_detach(self) — detach done in caller */
#endif
}

/* ── Signal handler (POSIX only) ──────────────────────────── */
#ifndef _WIN32
static void handle_signal(int sig)
{
    (void)sig;
    printf("\n[%s] Shutting down...\n", SERVER_NAME);
    if (server_fd != SOCK_INVALID) sock_close(server_fd);
    net_cleanup();
    exit(0);
}
#endif

/* ── Main ─────────────────────────────────────────────────── */
int main(void)
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    thread_t tid;

    if (net_init() != 0) {
        fprintf(stderr, "Network init failed\n");
        return 1;
    }

#ifdef _WIN32
    InitializeCriticalSection(&clients_mutex);
#else
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);
#endif

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == SOCK_INVALID) { perror("socket"); return 1; }

    int opt = 1;
    /* FIX: check setsockopt return value */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        sock_close(server_fd);
        net_cleanup();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        sock_close(server_fd);
        net_cleanup();
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        sock_close(server_fd);
        net_cleanup();
        return 1;
    }

    printf("[%s] server started on port %d\n", SERVER_NAME, PORT);
    printf("Waiting for connections... (Ctrl+C to stop)\n\n");

    while (1) {
        sock_t client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == SOCK_INVALID) continue;

        int count = 0;
        mutex_lock();
        for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i]) count++;
        mutex_unlock();

        if (count >= MAX_CLIENTS) {
            const char *full_msg = "Server full.\n";
            sock_write(client_fd, full_msg, (int)strlen(full_msg));
            sock_close(client_fd);
            continue;
        }

        client_t *cl = calloc(1, sizeof(client_t));
        if (!cl) { sock_close(client_fd); continue; }
        cl->fd = client_fd;
        add_client(cl);

        printf("New connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        /* FIX: check thread_create return value — clean up on failure */
        if (thread_create(&tid, handle_client, cl) != 0) {
            fprintf(stderr, "Failed to create thread for client\n");
            sock_close(client_fd);
            remove_client(cl->index);
            free(cl);
            continue;
        }
#ifdef _WIN32
        CloseHandle(tid);
#else
        /* FIX: detach here in caller, not inside the thread */
        pthread_detach(tid);
#endif
    }

    net_cleanup();
    return 0;
}
