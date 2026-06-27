#include <stdio.h>
#include <string.h>

#include "server.h"

int proxy_run(int, char **);

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "tiny-proxy --listen [address] [port]  :  standalone listener");
        fprintf(stderr, "tiny-proxy --help   :  help message");
        fprintf(stderr, "tiny-proxy  :  tcpsvd / socat mode");
        return 2;
    }
    if (argc > 1 && strcmp(argv[1], "--listen") == 0) {
        const char *host = argc > 2 ? argv[2] : "0.0.0.0";
        const char *port = argc > 3 ? argv[3] : "1080";
        return run_server(host, port);
    }
    return proxy_run(argc, argv);
}
