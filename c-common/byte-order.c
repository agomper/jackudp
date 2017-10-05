#include <string.h>

#include "byte-order.h"
#include "failure.h"
#include "float.h"
#include "int.h"
#include "print.h"

u16 hton16(u16 n)
{
  return htons(n);
}

u16 ntoh16(u16 n)
{
  return ntohs(n);
}

u32 hton32(u32 n)
{
  return htonl(n);
}

u32 ntoh32(u32 n)
{
  return ntohl(n);
}

u64 hton64(u64 n)
{
  u8 *ptr = (u8*)&n;
  return ((u64)hton32(*(u32*)ptr) << 32) + (hton32(*(u32*)(ptr + 4)));
}

u64 ntoh64(u64 n)
{
  u8 *ptr = (u8*)&n;
  return ((u64)ntoh32(*(u32*)ptr) << 32) + (ntoh32(*(u32*)(ptr + 4)));
}

void byte_order_confirm(void)
{
  u8 a[4] = {0,0,0,1};
  u32 b = ntoh_u32_from_buf(a);
  if(b != 1){
    eprintf("byte_order_confirm: incorrect machine word order\n");
    FAILURE;
  }
}

#define GENERATE_NC_TO_BUF(proc,size)		\
  void proc##size##_to_buf(u8 *buf,u##size n)	\
  {						\
    n = proc##size(n);				\
    memcpy(buf,&n,size/8);			\
  }

GENERATE_NC_TO_BUF(hton,16);
GENERATE_NC_TO_BUF(ntoh,16);
GENERATE_NC_TO_BUF(hton,32);
GENERATE_NC_TO_BUF(ntoh,32);
GENERATE_NC_TO_BUF(hton,64);
GENERATE_NC_TO_BUF(ntoh,64);

#define GENERATE_NC_FROM_BUF(proc,size)		\
  u##size proc##size##_from_buf(const u8 *buf)		\
  {						\
    u##size n;					\
    memcpy(&n,buf,size/8);			\
    return proc##size(n);			\
  }

GENERATE_NC_FROM_BUF(hton,16);
GENERATE_NC_FROM_BUF(ntoh,16);
GENERATE_NC_FROM_BUF(hton,32);
GENERATE_NC_FROM_BUF(ntoh,32);
GENERATE_NC_FROM_BUF(hton,64);
GENERATE_NC_FROM_BUF(ntoh,64);

#define GENERATE_NC_BUF(proc,size)		\
  void proc##size##_buf(u8 *dst,const u8 *src)	\
  {						\
    u##size n = proc##size##_from_buf(src);	\
    memcpy(dst,&n,size/8);			\
  }

GENERATE_NC_BUF(hton,16);
GENERATE_NC_BUF(ntoh,16);
GENERATE_NC_BUF(hton,32);
GENERATE_NC_BUF(ntoh,32);
GENERATE_NC_BUF(hton,64);
GENERATE_NC_BUF(ntoh,64);

#define GENERATE_TNC_TO_BUF(proc,tag,size)		\
  void proc##_##tag##size##_to_buf(u8 *buf,tag##size a)	\
  {							\
    n##size##_t n;					\
    n.tag = a;						\
    proc##size##_to_buf(buf,n.u);			\
  }

GENERATE_TNC_TO_BUF(hton,u,16);
GENERATE_TNC_TO_BUF(ntoh,u,16);
GENERATE_TNC_TO_BUF(hton,i,16);
GENERATE_TNC_TO_BUF(ntoh,i,16);
GENERATE_TNC_TO_BUF(hton,u,32);
GENERATE_TNC_TO_BUF(ntoh,u,32);
GENERATE_TNC_TO_BUF(hton,i,32);
GENERATE_TNC_TO_BUF(ntoh,i,32);
GENERATE_TNC_TO_BUF(hton,f,32);
GENERATE_TNC_TO_BUF(ntoh,f,32);
GENERATE_TNC_TO_BUF(hton,u,64);
GENERATE_TNC_TO_BUF(ntoh,u,64);
GENERATE_TNC_TO_BUF(hton,i,64);
GENERATE_TNC_TO_BUF(ntoh,i,64);
GENERATE_TNC_TO_BUF(hton,f,64);
GENERATE_TNC_TO_BUF(ntoh,f,64);

#define GENERATE_TNC_FROM_BUF(proc,tag,size)			\
  tag##size proc##_##tag##size##_from_buf(const u8 *buf)	\
  {								\
    n##size##_t n;						\
    n.u  = proc##size##_from_buf(buf);				\
    return n.tag;						\
  }

GENERATE_TNC_FROM_BUF(hton,u,16);
GENERATE_TNC_FROM_BUF(ntoh,u,16);
GENERATE_TNC_FROM_BUF(hton,i,16);
GENERATE_TNC_FROM_BUF(ntoh,i,16);
GENERATE_TNC_FROM_BUF(hton,u,32);
GENERATE_TNC_FROM_BUF(ntoh,u,32);
GENERATE_TNC_FROM_BUF(hton,i,32);
GENERATE_TNC_FROM_BUF(ntoh,i,32);
GENERATE_TNC_FROM_BUF(hton,f,32);
GENERATE_TNC_FROM_BUF(ntoh,f,32);
GENERATE_TNC_FROM_BUF(hton,u,64);
GENERATE_TNC_FROM_BUF(ntoh,u,64);
GENERATE_TNC_FROM_BUF(hton,i,64);
GENERATE_TNC_FROM_BUF(ntoh,i,64);
GENERATE_TNC_FROM_BUF(hton,f,64);
GENERATE_TNC_FROM_BUF(ntoh,f,64);

#define GENERATE_TNC(proc,tag)			\
  tag						\
  proc##_##tag(tag a)				\
  {						\
    proc##_##tag##_to_buf((u8 *)&a,a);		\
    return a;					\
  }

GENERATE_TNC(hton,u16);
GENERATE_TNC(hton,i16);
GENERATE_TNC(ntoh,u16);
GENERATE_TNC(ntoh,i16);
GENERATE_TNC(hton,u32);
GENERATE_TNC(hton,i32);
GENERATE_TNC(hton,f32);
GENERATE_TNC(hton,u64);
GENERATE_TNC(hton,i64);
GENERATE_TNC(hton,f64);
GENERATE_TNC(ntoh,u32);
GENERATE_TNC(ntoh,i32);
GENERATE_TNC(ntoh,f32);
GENERATE_TNC(ntoh,u64);
GENERATE_TNC(ntoh,i64);
GENERATE_TNC(ntoh,f64);
