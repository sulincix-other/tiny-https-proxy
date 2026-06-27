#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "server.h"
#include "utils.h"

extern int proxy_run(int, char **);

struct relay_in_args {
    SOCKET cfd;
    int pipe_w;
};

struct relay_out_args {
    SOCKET cfd;
    int pipe_r;
};

static unsigned __stdcall relay_in(void *arg) {
    struct relay_in_args *a = arg;
    char buf[BUF_SIZE];
    int n;
    while ((n = recv(a->cfd, buf, BUF_SIZE, 0)) > 0) {
        int pos = 0;
        while (pos < n) {
            int w = _write(a->pipe_w, buf + pos, n - pos);
            if (w <= 0) break;
            pos += w;
        }
    }
    _close(a->pipe_w);
    free(a);
    return 0;
}

static unsigned __stdcall relay_out(void *arg) {
    struct relay_out_args *a = arg;
    char buf[BUF_SIZE];
    int n;
    while ((n = _read(a->pipe_r, buf, BUF_SIZE)) > 0) {
        int pos = 0;
        while (pos < n) {
            int w = send(a->cfd, buf + pos, n - pos, 0);
            if (w <= 0) break;
            pos += w;
        }
    }
    _close(a->pipe_r);
    free(a);
    return 0;
}

static void handle_client(SOCKET cfd) {
    int stdin_pipe[2], stdout_pipe[2];
    if (_pipe(stdin_pipe, 4096, _O_BINARY) != 0) { closesocket(cfd); return; }
    if (_pipe(stdout_pipe, 4096, _O_BINARY) != 0) {
        _close(stdin_pipe[0]); _close(stdin_pipe[1]); closesocket(cfd); return;
    }

    struct relay_in_args *in_a = malloc(sizeof(*in_a));
    struct relay_out_args *out_a = malloc(sizeof(*out_a));
    if (!in_a || !out_a) {
        free(in_a); free(out_a);
        _close(stdin_pipe[0]); _close(stdin_pipe[1]);
        _close(stdout_pipe[0]); _close(stdout_pipe[1]);
        closesocket(cfd); return;
    }
    in_a->cfd = cfd; in_a->pipe_w = stdin_pipe[1];
    out_a->cfd = cfd; out_a->pipe_r = stdout_pipe[0];

    HANDLE hIn = (HANDLE)_beginthreadex(NULL, 0, relay_in, in_a, 0, NULL);
    HANDLE hOut = (HANDLE)_beginthreadex(NULL, 0, relay_out, out_a, 0, NULL);

    _dup2(stdin_pipe[0], 0);
    _dup2(stdout_pipe[1], 1);
    _close(stdin_pipe[0]);
    _close(stdout_pipe[1]);

    char *args[] = {"proxy", NULL};
    proxy_run(1, args);

    closesocket(cfd);
    _close(stdin_pipe[1]);
    _close(stdout_pipe[0]);
    if (hIn) { WaitForSingleObject(hIn, INFINITE); CloseHandle(hIn); }
    if (hOut) { WaitForSingleObject(hOut, INFINITE); CloseHandle(hOut); }
}

int run_server(const char *host, const char *port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err) { fprintf(stderr, "getaddrinfo error: %d\n", err); return -1; }

    SOCKET lfd = INVALID_SOCKET;
    for (struct addrinfo *rp = res; rp; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd == INVALID_SOCKET) continue;
        int opt = 1;
        setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
        if (bind(lfd, rp->ai_addr, (int)rp->ai_addrlen) == 0) break;
        closesocket(lfd);
        lfd = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (lfd == INVALID_SOCKET) { fprintf(stderr, "bind failed\n"); return -1; }

    listen(lfd, 128);
    fprintf(stderr, "listening on %s:%s\n", host, port);

    for (;;) {
        SOCKET cfd = accept(lfd, NULL, NULL);
        if (cfd == INVALID_SOCKET) continue;

        HANDLE th = (HANDLE)_beginthreadex(NULL, 0,
            (unsigned (__stdcall *)(void *))handle_client,
            (void *)(uintptr_t)cfd, 0, NULL);
        if (th) { CloseHandle(th); }
        else { closesocket(cfd); }
    }

    closesocket(lfd);
    return 0;
}
