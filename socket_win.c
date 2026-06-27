#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>

#define read _read
#define write _write

#include "socket.h"
#include "utils.h"

int handle_connection(SOCKET client_fd, int is_socket, const char *expected_b64);

int socket_init(){
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", err);
        return err;
    }
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    return 0;
}

void socket_end(){
    WSACleanup();
}

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
            fprintf(stderr, "connect to %s:%s failed: WSAGetLastError() = %d\n", host, port, WSAGetLastError());
            closesocket(fd);
            fd = INVALID_SOCKET;
        } else {
            fprintf(stderr, "socket creation failed: WSAGetLastError() = %d\n", WSAGetLastError());
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

SOCKET listen_on(const char *port) {
    struct addrinfo hints;
    struct addrinfo *list;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(NULL, port, &hints, &list);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo (listen) error: %d\n", err);
        return INVALID_SOCKET;
    }

    SOCKET fd = INVALID_SOCKET;
    rp = list;
    while (rp != NULL) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd != INVALID_SOCKET) {
            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
            if (rp->ai_family == AF_INET6) {
                int no = 0;
                setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&no, sizeof(no));
            }
            if (bind(fd, rp->ai_addr, (int)rp->ai_addrlen) == 0) {
                if (listen(fd, SOMAXCONN) == 0) {
                    break;
                }
            }
            fprintf(stderr, "bind/listen on port %s failed: WSAGetLastError() = %d\n", port, WSAGetLastError());
            closesocket(fd);
            fd = INVALID_SOCKET;
        }
        rp = rp->ai_next;
    }
    freeaddrinfo(list);
    return fd;
}

struct client_info {
    SOCKET fd;
    char expected_b64[512];
};

static unsigned __stdcall client_thread(void *arg) {
    struct client_info *info = (struct client_info *)arg;
    handle_connection(info->fd, 1, info->expected_b64);
    closesocket(info->fd);
    free(info);
    return 0;
}

int run_server(const char *port, const char *expected_b64) {
    SOCKET listen_fd = listen_on(port);
    if (listen_fd == INVALID_SOCKET) {
        fprintf(stderr, "Failed to listen on port %s\n", port);
        return 1;
    }
    fprintf(stderr, "Listening on port %s...\n", port);

    while (1) {
        struct sockaddr_storage addr;
        int addr_len = sizeof(addr);
        SOCKET client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addr_len);
        if (client_fd == INVALID_SOCKET) {
            fprintf(stderr, "accept failed: WSAGetLastError() = %d\n", WSAGetLastError());
            continue;
        }

        struct client_info *info = malloc(sizeof(struct client_info));
        if (!info) {
            closesocket(client_fd);
            continue;
        }
        info->fd = client_fd;
        if (expected_b64) {
            strncpy(info->expected_b64, expected_b64, sizeof(info->expected_b64) - 1);
            info->expected_b64[sizeof(info->expected_b64) - 1] = '\0';
        } else {
            info->expected_b64[0] = '\0';
        }

        HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, client_thread, info, 0, NULL);
        if (thread == NULL) {
            fprintf(stderr, "Failed to spawn thread for client\n");
            closesocket(client_fd);
            free(info);
        } else {
            CloseHandle(thread);
        }
    }

    closesocket(listen_fd);
    return 0;
}

void tunnel_ex(SOCKET client_fd, int is_socket, SOCKET remote_fd) {
    if (!is_socket) {
        tunnel(remote_fd);
        return;
    }

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(remote_fd, &read_fds);

        int err = select(0, &read_fds, NULL, NULL, NULL);
        if (err < 0)
            break;

        if (FD_ISSET(client_fd, &read_fds)) {
            char buf[BUF_SIZE];
            int n = recv(client_fd, buf, BUF_SIZE, 0);
            if (n <= 0)
                break;
            int pos = 0;
            while (pos < n) {
                int written = send(remote_fd, buf + pos, n - pos, 0);
                if (written <= 0)
                    return;
                pos += written;
            }
        }
        if (FD_ISSET(remote_fd, &read_fds)) {
            char buf[BUF_SIZE];
            int n = recv(remote_fd, buf, BUF_SIZE, 0);
            if (n <= 0)
                break;
            int pos = 0;
            while (pos < n) {
                int written = send(client_fd, buf + pos, n - pos, 0);
                if (written <= 0)
                    return;
                pos += written;
            }
        }
    }
}

#endif
