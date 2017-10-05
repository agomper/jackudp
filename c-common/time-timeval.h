#ifndef _COMMON_TIME_TIMEVAL_H
#define _COMMON_TIME_TIMEVAL_H

#include <sys/time.h>

#include "int.h"
#include "float.h"

struct timeval usec_to_timeval(u64 usec);
f64 timeval_to_real(struct timeval tv);
struct timeval real_to_timeval(f64 d);
int timeval_subtract(struct timeval *result, struct timeval x, struct timeval y);

#endif
