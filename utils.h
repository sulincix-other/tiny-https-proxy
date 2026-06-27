#ifndef UTILS_H
#define UTILS_H

#include "socket.h"

#define BUF_SIZE 8192

int read_line(int fd, char *buf, int max);
int my_recv(SOCKET fd, int is_socket, void *buf, int len);
int my_send(SOCKET fd, int is_socket, const void *buf, int len);
int read_line_ex(SOCKET fd, int is_socket, char *buf, int max);
char *parse_host_port(char *url, char *host, int host_size,
                      char *port, int port_size);
void base64_encode(const unsigned char *in, int in_len,
                   char *out, int out_max);
int check_auth(const char *val, const char *expected_b64);

#endif
