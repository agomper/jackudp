#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "client.h"
#include "memory.h"
#include "network.h"
#include "print.h"

client_register_t *alloc_client_register(i32 n)
{
  client_register_t *r = xmalloc(sizeof(client_register_t));
  r->n = n;
  r->c = xmalloc(n * sizeof(client_t));
  i32 i;
  for(i = 0; i < r->n; i++) {
    r->c[i].in_use = 0;
    r->c[i].mask = 0;
  }
  return r;
}

void print_client_register(FILE *fp, const client_register_t *r, i32 mask)
{
  i32 i;
  fprintf(fp, "print_client_register:\n");
  for(i = 0; i < r->n; i++) {
    if(r->c[i].in_use &&(( r->c[i].mask & mask)|| mask == -1)) {
      fprintf(fp, "%d in use, mask = %d\n", i, r->c[i].mask);
      print_sockaddr_in(fp, r->c[i].address);
    }
  }
}

void free_client_register(client_register_t *r)
{
  free(r->c);
  free(r);
}

void edit_client_register(client_register_t *r,
			  struct sockaddr_in addr,
			  i32 mask)
{
  dprintf("edit_client_register: mask=%d\n", mask);
  i32 i;
  for(i = 0; i < r->n; i++) {
    if(r->c[i].in_use && sockaddr_in_equal(&addr, &(r->c[i].address))) {
      if(mask < 0) {
	r->c[i].in_use = 0;
	r->c[i].mask = 0;
      } else {
	r->c[i].mask = mask;
      }
      return;
    }
  }
  if(mask < 0) {
    eprintf("edit_client_register: Illegal unregistered client\n");
    return;
  }
  for(i = 0; i < r->n; i++) {
    if(! r->c[i].in_use) {
      memcpy(&(r->c[i].address), &addr, sizeof(addr));
      r->c[i].mask = mask;
      r->c[i].in_use = 1;
      return;
    }
  }
  eprintf("edit_client_register: Client dropped.\n");
}

void sendto_client_register(int fd,
			    const client_register_t *r,
			    const u8 *packet, i32 packet_sz,
			    i32 mask)
{
  if(packet_sz) {
    i32 i;
    for(i = 0; i < r->n; i++) {
      if(r->c[i].in_use &&(r->c[i].mask & mask)) {
        sendto_exactly(fd, packet, packet_sz, r->c[i].address);
      }
    }
  }
}
