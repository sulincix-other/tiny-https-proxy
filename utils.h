#ifndef UTILS_H
#define UTILS_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
#endif

#define BUF_SIZE 8192

int read_line(int fd, char *buf, int max);
#ifndef _WIN32
int copy_data(SOCKET from_fd, SOCKET to_fd);
#endif
SOCKET connect_to(const char *host, const char *port);
void tunnel(SOCKET remote_fd);
char *parse_host_port(char *url, char *host, int host_size,
                      char *port, int port_size);
void base64_encode(const unsigned char *in, int in_len,
                   char *out, int out_max);
int check_auth(const char *val, const char *expected_b64);

#endif
