#ifndef SOCKET_H
#define SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
typedef int SOCKET;
#define INVALID_SOCKET -1
#define closesocket(s) close(s)
#endif

int socket_init();
void socket_end();

SOCKET connect_to(const char *host, const char *port);
void tunnel(SOCKET remote_fd);

#endif
