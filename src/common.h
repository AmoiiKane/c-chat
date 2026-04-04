#ifndef COMMON_H
#define COMMON_H

/* ── Platform detection ───────────────────────────────────── */
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")

  typedef SOCKET        sock_t;
  typedef HANDLE        thread_t;

  #define SOCK_INVALID  INVALID_SOCKET
  #define sock_close(s) closesocket(s)
  #define sock_read(s,b,n)  recv(s, b, n, 0)
  #define sock_write(s,b,n) send(s, b, n, 0)
  #define sock_error()  WSAGetLastError()

  /* Windows thread wrapper */
  #define THREAD_FUNC   DWORD WINAPI
  #define thread_create(t, fn, arg) \
    (*(t) = CreateThread(NULL, 0, fn, arg, 0, NULL), *(t) != NULL ? 0 : -1)
  #define thread_detach(t) CloseHandle(t)

#else
  /* POSIX */
  #include <unistd.h>
  #include <pthread.h>
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <signal.h>

  typedef int           sock_t;
  typedef pthread_t     thread_t;

  #define SOCK_INVALID  (-1)
  #define sock_close(s) close(s)
  #define sock_read(s,b,n)  read(s, b, n)
  #define sock_write(s,b,n) write(s, b, n)
  #define sock_error()  errno

  #define THREAD_FUNC   void *
  #define thread_create(t, fn, arg) pthread_create(t, NULL, fn, arg)
  #define thread_detach(t) pthread_detach(t)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT            8080
#define MAX_CLIENTS     32
#define BUFFER_SIZE     2048
#define USERNAME_SIZE   32
#define SERVER_NAME     "c-chat"

typedef struct {
    char username[USERNAME_SIZE];
    char buffer[BUFFER_SIZE];
} client_msg_t;

/* ── Platform init/cleanup ────────────────────────────────── */
static inline int net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa);
#else
    return 0;
#endif
}

static inline void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

#endif /* COMMON_H */
