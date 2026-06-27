#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "socket.h"
#include "utils.h"

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
            closesocket(fd);
            fd = INVALID_SOCKET;
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

void socket_init(){}
void socket_end(){}