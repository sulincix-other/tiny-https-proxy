#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "platform.h"
#include "server.h"

extern int proxy_run(int, char **);

static int lfd;

int run_server(const char *host, const char *port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); return -1; }

    lfd = -1;
    for (struct addrinfo *rp = res; rp; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd < 0) continue;
        int opt = 1;
        setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(lfd);
        lfd = -1;
    }
    freeaddrinfo(res);
    if (lfd < 0) { fprintf(stderr, "bind failed\n"); return -1; }

    listen(lfd, 128);
    fprintf(stderr, "listening on %s:%s\n", host, port);
    signal(SIGCHLD, SIG_IGN);

    struct pollfd pfd = {.fd = lfd, .events = POLLIN};

    for (;;) {
        pfd.revents = 0;
        if (poll(&pfd, 1, -1) < 0) break;

        for (;;) {
            int cfd = accept(lfd, NULL, NULL);
            if (cfd < 0) break;

            pid_t pid = fork();
            if (pid == 0) {
                close(lfd);
                dup2(cfd, 0);
                dup2(cfd, 1);
                if (cfd > 1) close(cfd);
                signal(SIGCHLD, SIG_DFL);
                char *args[] = {"proxy", NULL};
                int status = proxy_run(1, args);
                _exit(status);
            }
            close(cfd);
        }
    }

    close(lfd);
    return 0;
}
