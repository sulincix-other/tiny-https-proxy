#ifndef SOCKET_PLATFORM_H
#define SOCKET_PLATFORM_H

#include <winsock2.h>
#include <ws2tcpip.h>

int socket_init();
void socket_end();

SOCKET connect_to(const char *host, const char *port);
void tunnel(int remote_fd);

#endif
