#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>

#include "socket.h"
#include "utils.h"

int handle_connection(SOCKET client_fd, int is_socket, const char *expected_b64);

static int copy_data(int from_fd, int to_fd) {
    char buf[BUF_SIZE];
    int n = read(from_fd, buf, BUF_SIZE);

    if (n <= 0)
        return -1;

    int pos = 0;
    while (pos < n) {
        int written = write(to_fd, buf + pos, n - pos);
        if (written <= 0)
            return -1;
        pos += written;
    }
    return 0;
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
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return INVALID_SOCKET;
    }

    SOCKET fd = INVALID_SOCKET;
    rp = list;
    while (rp != NULL) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd != INVALID_SOCKET) {
            if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
                break;
            fprintf(stderr, "connect to %s:%s failed: errno = %d (%s)\n", host, port, errno, strerror(errno));
            closesocket(fd);
            fd = INVALID_SOCKET;
        } else {
            fprintf(stderr, "socket creation failed: errno = %d (%s)\n", errno, strerror(errno));
        }
        rp = rp->ai_next;
    }
    freeaddrinfo(list);
    return fd;
}

void tunnel(SOCKET remote_fd) {
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(0, &read_fds);
        FD_SET(remote_fd, &read_fds);

        int nfds = (remote_fd > 0 ? remote_fd : 0) + 1;

        if (select(nfds, &read_fds, NULL, NULL, NULL) < 0)
            break;

        if (FD_ISSET(0, &read_fds)) {
            if (copy_data(0, remote_fd) < 0)
                break;
        }
        if (FD_ISSET(remote_fd, &read_fds)) {
            if (copy_data(remote_fd, 1) < 0)
                break;
        }
    }
}

int socket_init(){
    return 0;
}

void socket_end(){}

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
        fprintf(stderr, "getaddrinfo (listen) error: %s\n", gai_strerror(err));
        return INVALID_SOCKET;
    }

    SOCKET fd = INVALID_SOCKET;
    rp = list;
    while (rp != NULL) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd != INVALID_SOCKET) {
            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            if (rp->ai_family == AF_INET6) {
                int no = 0;
                setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
            }
            if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
                if (listen(fd, SOMAXCONN) == 0) {
                    break;
                }
            }
            fprintf(stderr, "bind/listen on port %s failed: errno = %d (%s)\n", port, errno, strerror(errno));
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

static void *client_thread(void *arg) {
    struct client_info *info = (struct client_info *)arg;
    handle_connection(info->fd, 1, info->expected_b64);
    closesocket(info->fd);
    free(info);
    return NULL;
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
        socklen_t addr_len = sizeof(addr);
        SOCKET client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addr_len);
        if (client_fd == INVALID_SOCKET) {
            fprintf(stderr, "accept failed: errno = %d (%s)\n", errno, strerror(errno));
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

        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        int err = pthread_create(&thread, &attr, client_thread, info);
        pthread_attr_destroy(&attr);
        if (err != 0) {
            fprintf(stderr, "Failed to spawn thread for client\n");
            closesocket(client_fd);
            free(info);
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

    int nfds = (client_fd > remote_fd ? client_fd : remote_fd) + 1;
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(remote_fd, &read_fds);

        int err = select(nfds, &read_fds, NULL, NULL, NULL);
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