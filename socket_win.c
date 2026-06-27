#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <process.h>
#include <windows.h>

#define read _read
#define write _write

#include <winsock2.h>
#include <ws2tcpip.h>

#include "socket.h"
#include "utils.h"

SOCKET connect_to(const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *list;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port, &hints, &list);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo error: %d\n", err);
        return INVALID_SOCKET;
    }

    SOCKET fd = INVALID_SOCKET;
    rp = list;
    while (rp != NULL) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd != INVALID_SOCKET) {
            if (connect(fd, rp->ai_addr, (int)rp->ai_addrlen) == 0)
                break;
            closesocket(fd);
            fd = INVALID_SOCKET;
        }
        rp = rp->ai_next;
    }
    freeaddrinfo(list);
    return fd;
}

struct tunnel_args {
    SOCKET remote_fd;
};

static unsigned __stdcall tunnel_recv_thread(void *arg) {
    SOCKET remote_fd = ((struct tunnel_args *)arg)->remote_fd;
    free(arg);
    char buf[BUF_SIZE];
    while (1) {
        int n = recv(remote_fd, buf, BUF_SIZE, 0);
        if (n <= 0)
            break;
        int pos = 0;
        while (pos < n) {
            int written = _write(1, buf + pos, n - pos);
            if (written <= 0)
                break;
            pos += written;
        }
    }
    closesocket(remote_fd);
    exit(0);
    return 0;
}

void tunnel(SOCKET remote_fd) {
    struct tunnel_args *args = malloc(sizeof(struct tunnel_args));
    if (!args) return;
    args->remote_fd = remote_fd;

    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, tunnel_recv_thread, args, 0, NULL);

    char buf[BUF_SIZE];
    while (1) {
        int n = _read(0, buf, BUF_SIZE);
        if (n <= 0)
            break;
        int pos = 0;
        while (pos < n) {
            int written = send(remote_fd, buf + pos, n - pos, 0);
            if (written <= 0)
                goto end;
            pos += written;
        }
    }

end:
    closesocket(remote_fd);
    if (thread) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
}

#endif
