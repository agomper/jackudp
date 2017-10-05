#ifndef _COMMON_NETWORK_H
#define _COMMON_NETWORK_H

#include <sys/types.h>
#include <sys/socket.h>

#include "int.h"

int socket_udp(int flags);
void init_sockaddr_in(struct sockaddr_in *name, const char *hostname, uint16_t port);
void init_broadcast_sockaddr_in(struct sockaddr_in *name, uint16_t port);
int sockaddr_in_equal(const struct sockaddr_in *a, const struct sockaddr_in *b);
void print_sockaddr_in(FILE *fp, struct sockaddr_in a);
void bind_inet(int fd, const char *hostname, int port);
void connect_inet(int fd, const char *hostname, int port);
int xsendto(int fd, const void *data, size_t n, int flags, struct sockaddr *addr, socklen_t length);
void sendto_exactly(int fd, const u8 *data, int n, struct sockaddr_in address);
int xrecv(int fd, void *buf, size_t n, int flags);
void recv_exactly(int fd, void *buf, size_t n, int flags);
int xrecvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *length);
void recvfrom_exactly(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *length);
int fd_wait(int fd, unsigned long timeout);

#endif
