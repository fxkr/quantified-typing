#include <sys/time.h>

#include "util.h"

void timespec_subtract(struct timespec *out, struct timespec *a, struct timespec *b)
{
  out->tv_sec = a->tv_sec - b->tv_sec;
  out->tv_nsec = a->tv_nsec - b->tv_nsec;

  if (a->tv_nsec < b->tv_nsec) {
    out->tv_nsec += 1e9;
    out->tv_sec -= 1;
  }
}
