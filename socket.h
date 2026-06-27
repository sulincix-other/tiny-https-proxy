#ifndef SOCKET_H
#define SOCKET_H

#ifndef _WIN32
#define INVALID_SOCKET -1
#define SOCKET int
#define closesocket close
#endif

SOCKET connect_to(const char *host, const char *port);
void tunnel(SOCKET remote_fd);

#endif
