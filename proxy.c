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
#define close closesocket
#else
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "utils.h"
#include "socket.h"

int main(int argc, char *argv[]) {
    socket_init();
    char expected_b64[512] = "";
    int  authenticated = argc != 3;

    if (argc == 3) {
        char cred[256];
        int cred_len = snprintf(cred, sizeof(cred), "%s:%s", argv[1], argv[2]);
        base64_encode((unsigned char *)cred, cred_len, expected_b64, sizeof(expected_b64));
    }

    char line[BUF_SIZE];
    char method[64];
    char url[BUF_SIZE];
    char host[256];
    char port[16];
    char headers[BUF_SIZE];
    int  header_len = 0;

    int n = read_line(0, line, BUF_SIZE);
    if (n <= 0)
        return 1;

    if (sscanf(line, "%63s %1023s", method, url) < 2)
        return 1;

    headers[0] = '\0';
    while (1) {
        n = read_line(0, line, BUF_SIZE);
        if (n <= 0)
            break;
        if (line[0] == '\r' || line[0] == '\0')
            break;

        if (strncasecmp(line, "Proxy-Authorization:", 20) == 0) {
            const char *val = line + 20;
            while (*val == ' ' || *val == '\t') val++;

            char valbuf[256];
            int vlen = strlen(val);
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
        }
    }

    if (!authenticated) {
        const char *resp = "HTTP/1.0 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"proxy\"\r\n\r\n";
        write(1, resp, strlen(resp));
        return 1;
    }

    if (strcmp(method, "CONNECT") == 0) {
        if (sscanf(url, "%255[^:]:%15s", host, port) < 2)
            return 1;

        fprintf(stderr, "Connect: %s:%s\n", host, port);
        SOCKET remote = connect_to(host, port);
        if (remote == INVALID_SOCKET) {
            write(1, "HTTP/1.0 502 Bad Gateway\r\n\r\n", 28);
            return 1;
        }

        write(1, "HTTP/1.0 200 Connection established\r\n\r\n", 39);
        tunnel(remote);
        close(remote);
        fprintf(stderr, "Disconnect: %s:%s\n", host, port);
        socket_end();
        return 0;
    }

    char *path = parse_host_port(url, host, sizeof(host),
                                 port, sizeof(port));

    SOCKET remote = connect_to(host, port);
    if (remote == INVALID_SOCKET) {
        write(1, "HTTP/1.0 502 Bad Gateway\r\n\r\n", 28);
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

    tunnel(remote);
    close(remote);
    socket_end();
    return 0;
}
