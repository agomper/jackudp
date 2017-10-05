#ifndef _COMMON_BYTE_ORDER_H
#define _COMMON_BYTE_ORDER_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include "float.h"
#include "int.h"

typedef union
{
  u16 u;
  i16 i;
}
n16_t;

typedef union
{
  u32 u;
  i32 i;
  f32 f;
}
n32_t;

typedef union
{
  u64 u;
  i64 i;
  f64 f;
}
n64_t;

void byte_order_confirm(void);
void hton16_to_buf(u8 *buf, u16 n);
void ntoh16_to_buf(u8 *buf, u16 n);
void hton32_to_buf(u8 *buf, u32 n);
void ntoh32_to_buf(u8 *buf, u32 n);
void hton64_to_buf(u8 *buf, u64 n);
void ntoh64_to_buf(u8 *buf, u64 n);
u16  hton16_from_buf(const u8 *buf);
u16  ntoh16_from_buf(const u8 *buf);
u32  hton32_from_buf(const u8 *buf);
u32  ntoh32_from_buf(const u8 *buf);
u64  hton64_from_buf(const u8 *buf);
u64  ntoh64_from_buf(const u8 *buf);
void hton16_buf(u8 *dst, const u8 *src);
void ntoh16_buf(u8 *dst, const u8 *src);
void hton32_buf(u8 *dst, const u8 *src);
void ntoh32_buf(u8 *dst, const u8 *src);
void hton64_buf(u8 *dst, const u8 *src);
void ntoh64_buf(u8 *dst, const u8 *src);
void hton_u16_to_buf(u8 *buf, u16 a);
void ntoh_u16_to_buf(u8 *buf, u16 a);
void hton_i16_to_buf(u8 *buf, i16 a);
void ntoh_i16_to_buf(u8 *buf, i16 a);
void hton_u32_to_buf(u8 *buf, u32 a);
void ntoh_u32_to_buf(u8 *buf, u32 a);
void hton_i32_to_buf(u8 *buf, i32 a);
void ntoh_i32_to_buf(u8 *buf, i32 a);
void hton_f32_to_buf(u8 *buf, f32 a);
void ntoh_f32_to_buf(u8 *buf, f32 a);
void hton_u64_to_buf(u8 *buf, u64 a);
void ntoh_u64_to_buf(u8 *buf, u64 a);
void hton_i64_to_buf(u8 *buf, i64 a);
void ntoh_i64_to_buf(u8 *buf, i64 a);
void hton_f64_to_buf(u8 *buf, f64 a);
void ntoh_f64_to_buf(u8 *buf, f64 a);
u16  hton_u16_from_buf(const u8 *buf);
u16  ntoh_u16_from_buf(const u8 *buf);
i16  hton_i16_from_buf(const u8 *buf);
i16  ntoh_i16_from_buf(const u8 *buf);
u32  hton_u32_from_buf(const u8 *buf);
u32  ntoh_u32_from_buf(const u8 *buf);
i32  hton_i32_from_buf(const u8 *buf);
i32  ntoh_i32_from_buf(const u8 *buf);
f32  hton_f32_from_buf(const u8 *buf);
f32  ntoh_f32_from_buf(const u8 *buf);
u64  hton_u64_from_buf(const u8 *buf);
u64  ntoh_u64_from_buf(const u8 *buf);
i64  hton_i64_from_buf(const u8 *buf);
i64  ntoh_i64_from_buf(const u8 *buf);
f64  hton_f64_from_buf(const u8 *buf);
f64  ntoh_f64_from_buf(const u8 *buf);
u16  hton_u16(u16 a);
i16  hton_i16(i16 a);
u16  ntoh_u16(u16 a);
i16  ntoh_i16(i16 a);
u32  hton_u32(u32 a);
i32  hton_i32(i32 a);
f32  hton_f32(f32 a);
u64  hton_u64(u64 a);
i64  hton_i64(i64 a);
f64  hton_f64(f64 a);
u32  ntoh_u32(u32 a);
i32  ntoh_i32(i32 a);
f32  ntoh_f32(f32 a);
u64  ntoh_u64(u64 a);
i64  ntoh_i64(i64 a);
f64  ntoh_f64(f64 a);

#endif
