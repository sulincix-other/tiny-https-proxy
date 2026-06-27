#ifndef SOCKET_PLATFORM_H
#define SOCKET_PLATFORM_H

#define SOCKET int
#define INVALID_SOCKET -1

int socket_init();
void socket_end();

int connect_to(const char *host, const char *port);
void tunnel(int remote_fd);
#endif
