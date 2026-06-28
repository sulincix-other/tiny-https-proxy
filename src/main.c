#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "server.h"
#include "socket.h"

int proxy_run(int, char **);
const char *auth = NULL;

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "tiny-proxy --auth [username:password]  : authenticate\n");
        fprintf(stderr, "tiny-proxy --listen [address] [port]  :  standalone listener\n");
        fprintf(stderr, "tiny-proxy --help   :  help message\n");
        fprintf(stderr, "tiny-proxy  :  tcpsvd / socat mode\n");
        return 2;
    }
    const char *host;
    const char *port;
    for(int i=1; argv[i]; i++){
        if (strcmp(argv[i], "--listen") == 0) {
            if (argv[i+1] && argv[i+2]){
                host = argv[i+1];
                port = argv[i+2];
            } else {
                host = "0.0.0.0";
                port = "1080";
            }
        }
        if (strcmp(argv[i], "--auth") == 0) {
            auth = argc > i ? argv[i+1] : "admin:admin";
        }
    }
    if(host){
        if (socket_init() != 0) {
            return 1;
        }
        atexit(socket_end);
        return run_server(host, port);
    }
    return proxy_run(argc, argv);
}
