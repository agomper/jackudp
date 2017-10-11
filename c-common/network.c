#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>

#include "int.h"
#include "failure.h"
#include "time-timeval.h"

int socket_udp(int flags)
{
  //int socket(int domain, int type, int protocol)
  int fd = socket(AF_INET, SOCK_DGRAM, flags);
  if(fd < 0) {
    perror("socket_udp: socket() failed");
    FAILURE;
  }
  return fd;
}

void init_sockaddr_in(struct sockaddr_in *name, const char *hostname, uint16_t port)
{
  name->sin_family = AF_INET;
  name->sin_port = htons(port);
  if(hostname) {
    struct hostent *hostinfo = gethostbyname(hostname);
    if (hostinfo == NULL) {
      fprintf(stderr, "init_sockaddr_in: unknown host: %s.\n", hostname);
      FAILURE;
    }
    name->sin_addr = *(struct in_addr *) hostinfo->h_addr_list[0];
  } else {
    name->sin_addr.s_addr = htonl(INADDR_ANY);
  }
}

void init_broadcast_sockaddr_in(struct sockaddr_in *name, uint16_t port)
{
  name->sin_family = AF_INET;
  name->sin_port = htons(port);
  name->sin_addr.s_addr = htonl(INADDR_BROADCAST);
}

int sockaddr_in_equal(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
  return(( a->sin_family == b->sin_family)&&
	 (a->sin_addr.s_addr == b->sin_addr.s_addr)&&
	 (a->sin_port == b->sin_port));
}

void print_sockaddr_in(FILE *fp, struct sockaddr_in a)
{
  fprintf(fp, "sin_family      %d\n", a.sin_family);
  fprintf(fp, "sin_port        %d\n", a.sin_port);
  fprintf(fp, "sin_addr.s_addr %d\n", a.sin_addr.s_addr);
}

void bind_inet(int fd, const char *hostname, int port)
{
  struct sockaddr_in name;
  init_sockaddr_in(&name, hostname, port);
  //int bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
  int err = bind(fd, (struct sockaddr *)&name, sizeof(name));
  if(err < 0) {
    perror("bind_inet: bind() failed");
    FAILURE;
  }
}

void connect_inet(int fd, const char *hostname, int port)
{
  struct sockaddr_in name;
  init_sockaddr_in(&name, hostname, port);
  int err = connect(fd, (struct sockaddr *)&name, sizeof(name));
  if(err < 0) {
    perror("connect_inet: bind() failed");
    FAILURE;
  }
}

int xsendto(int fd, const void *data, size_t n, int flags, struct sockaddr *addr, socklen_t length)
{
  int err = sendto(fd, data, n, flags, addr, length);
  if(err < 0) {
    perror("sendto() failed");
    FAILURE;
  }
  return err;
}

void sendto_exactly(int fd, const u8 *data, int n, struct sockaddr_in address)
{
  int err = xsendto(fd, data, n, 0, (struct sockaddr *)&address, sizeof(address));
  if(err != n) {
    fprintf(stderr, "sendto_exactly: partial write\n");
    FAILURE;
  }
}

int xrecv(int fd, void *buf, size_t n, int flags)
{
  int err = recv(fd, buf, n, flags);
  if(err < 0) {
    perror("recv() failed");
    FAILURE;
  }
  return err;
}

void recv_exactly(int fd, void *buf, size_t n, int flags)
{
  int err = (int) xrecv(fd, buf, n, flags);
  if(err != n) {
    fprintf(stderr, "recv_exactly: partial recv (%d != %ul)\n", err, (int) n);
    FAILURE;
  }
}

int xrecvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *length)
{
  int err = recvfrom(fd, buf, n, flags, addr, length);
  if(err < 0) {
    perror("recvfrom() failed");
    FAILURE;
  }
  return err;
}

void recvfrom_exactly(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *length)
{
  int err = (int) xrecvfrom(fd, buf, n, flags, addr, length);
  if(err != n) {
    fprintf(stderr, "recvfrom_exactly: partial recv (%d != %d)\n", err, (int) n);
    FAILURE;
  }
}

/* Wait at most `timeout' usecs for `fd' to become readable.  Returns
   0 on timeout, 1 if data is available. */

int fd_wait(int fd, unsigned long timeout)
{
  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd, &set);

  struct timeval t;
  t = usec_to_timeval(timeout);

  int err;
  err = select(FD_SETSIZE, &set, NULL, NULL, &t);

  if(err == -1) {
    fprintf(stderr, "fd_wait: select failed, %d\n", err);
    FAILURE;
  }
  return err;
}
