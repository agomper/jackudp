#ifndef _COMMON_CLIENT_H
#define _COMMON_CLIENT_H

#include "int.h"

typedef struct
{
  struct sockaddr_in address ;
  i32 mask ;
  i8 in_use ;
} client_t ;

typedef struct
{
  client_t *c ;
  i32 n ;
} client_register_t ;

client_register_t *alloc_client_register(i32 n);
void print_client_register(FILE *fp, const client_register_t *r, i32 mask);
void free_client_register(client_register_t *r);
void edit_client_register(client_register_t *r, struct sockaddr_in addr, i32 mask);
void sendto_client_register(int fd, const client_register_t *r, const u8 *packet, i32 packet_sz, i32 mask);

#endif
