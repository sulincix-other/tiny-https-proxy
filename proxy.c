#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define read _read
#define write _write
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "utils.h"
#include "socket.h"

static int verbose = 0;

int handle_connection(SOCKET client_fd, int is_socket, const char *expected_b64) {
    int authenticated = (expected_b64 == NULL || expected_b64[0] == '\0');

    char line[BUF_SIZE];
    char method[64];
    char url[BUF_SIZE];
    char host[256];
    char port[16];
    char headers[BUF_SIZE];
    int  header_len = 0;

    int n = read_line_ex(client_fd, is_socket, line, BUF_SIZE);
    if (n <= 0)
        return 1;

    if (sscanf(line, "%63s %1023s", method, url) < 2)
        return 1;

    headers[0] = '\0';
    while (1) {
        n = read_line_ex(client_fd, is_socket, line, BUF_SIZE);
        if (n <= 0)
            break;
        if (line[0] == '\r' || line[0] == '\0')
            break;

        if (expected_b64 && expected_b64[0] != '\0' && strncasecmp(line, "Proxy-Authorization:", 20) == 0) {
            const char *val = line + 20;
            while (*val == ' ' || *val == '\t') val++;

            char valbuf[256];
            int vlen = (int)strlen(val);
            while (vlen > 0 && (val[vlen-1] == '\r' || val[vlen-1] == '\n'))
                vlen--;
            if (vlen >= (int)sizeof(valbuf))
                vlen = sizeof(valbuf) - 1;
            memcpy(valbuf, val, vlen);
            valbuf[vlen] = '\0';

            authenticated = check_auth(valbuf, expected_b64);
            continue;
        }

        if (header_len + n < (int)sizeof(headers)) {
            memcpy(headers + header_len, line, n);
            header_len += n;
        } else {
            fprintf(stderr, "Warning: client header buffer full, ignoring additional header line: %s", line);
        }
    }

    if (!authenticated) {
        const char *resp = "HTTP/1.0 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"proxy\"\r\n\r\n";
        my_send(client_fd, is_socket, resp, (int)strlen(resp));
        return 1;
    }

    if (strcmp(method, "CONNECT") == 0) {
        if (sscanf(url, "%255[^:]:%15s", host, port) < 2)
            return 1;

        if (verbose) {
            fprintf(stderr, "Connect: CONNECT %s:%s\n", host, port);
        } else {
            fprintf(stderr, "Connect: %s:%s\n", host, port);
        }
        SOCKET remote = connect_to(host, port);
        if (remote == INVALID_SOCKET) {
            my_send(client_fd, is_socket, "HTTP/1.0 502 Bad Gateway\r\n\r\n", 28);
            return 1;
        }

        my_send(client_fd, is_socket, "HTTP/1.0 200 Connection established\r\n\r\n", 39);
        tunnel_ex(client_fd, is_socket, remote);
        closesocket(remote);
        if (verbose) {
            fprintf(stderr, "Disconnect: CONNECT %s:%s\n", host, port);
        } else {
            fprintf(stderr, "Disconnect: %s:%s\n", host, port);
        }
        return 0;
    }

    char *path = parse_host_port(url, host, sizeof(host),
                                 port, sizeof(port));

    if (verbose) {
        fprintf(stderr, "Connect: %s %s:%s\n", method, host, port);
    }
    SOCKET remote = connect_to(host, port);
    if (remote == INVALID_SOCKET) {
        my_send(client_fd, is_socket, "HTTP/1.0 502 Bad Gateway\r\n\r\n", 28);
        return 1;
    }

    {
        char request[BUF_SIZE];
        int len = snprintf(request, sizeof(request),
                           "%s %s HTTP/1.0\r\n", method, path);
        send(remote, request, len, 0);
    }

    if (header_len > 0)
        send(remote, headers, header_len, 0);

    send(remote, "\r\n", 2, 0);

    tunnel_ex(client_fd, is_socket, remote);
    closesocket(remote);
    if (verbose) {
        fprintf(stderr, "Disconnect: %s %s:%s\n", method, host, port);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (socket_init() != 0) {
        return 1;
    }
    atexit(socket_end);

    const char *listen_port = NULL;
    const char *username = NULL;
    const char *password = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--listen") == 0) {
            if (i + 1 < argc) {
                listen_port = argv[i+1];
                i++;
            } else {
                fprintf(stderr, "Error: %s requires a port argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            if (!username) {
                username = argv[i];
            } else if (!password) {
                password = argv[i];
            } else {
                fprintf(stderr, "Error: Unexpected argument '%s'\n", argv[i]);
                return 1;
            }
        }
    }

    if ((username && !password) || (!username && password)) {
        fprintf(stderr, "Error: Both username and password must be provided for authentication\n");
        return 1;
    }

    char expected_b64[512] = "";
    if (username && password) {
        char cred[256];
        int cred_len = snprintf(cred, sizeof(cred), "%s:%s", username, password);
        base64_encode((unsigned char *)cred, cred_len, expected_b64, sizeof(expected_b64));
    }

    if (listen_port != NULL) {
        return run_server(listen_port, expected_b64);
    } else {
        return handle_connection(0, 0, expected_b64);
    }
}
