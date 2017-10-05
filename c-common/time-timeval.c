#include <stdlib.h>
#include <math.h>

#include "int.h"
#include "float.h"
#include "time-timeval.h"

struct timeval usec_to_timeval(u64 usec)
{
  struct timeval t;
  ldiv_t d  = ldiv(usec, 1000000L);
  t.tv_sec  = d.quot;
  t.tv_usec = d.rem;
  return t;
}

f64 timeval_to_real(struct timeval tv)
{
  return (f64)tv.tv_sec + ((f64)tv.tv_usec / 1000000.0);
}

struct timeval real_to_timeval(f64 d)
{
  struct timeval t;
  t.tv_sec  = (long)floor(d);
  t.tv_usec = (long)ceil((d - (f64)t.tv_sec) * 1000000.0);
  return t;
}

int timeval_subtract(struct timeval *result,
		     struct timeval x,
		     struct timeval y)
{
  if (x.tv_usec < y.tv_usec) {
    int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
    y.tv_usec -= 1000000 * nsec;
    y.tv_sec += nsec;
  }
  if (x.tv_usec - y.tv_usec > 1000000) {
    int nsec = (x.tv_usec - y.tv_usec) / 1000000;
    y.tv_usec += 1000000 * nsec;
    y.tv_sec -= nsec;
  }
  result->tv_sec = x.tv_sec - y.tv_sec;
  result->tv_usec = x.tv_usec - y.tv_usec;
  return x.tv_sec < y.tv_sec;
}
