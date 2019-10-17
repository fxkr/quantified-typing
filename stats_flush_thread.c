#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "stats_thread.h"

#include "stats_flush_thread.h"

/* May be overwritten via $INTERVAL environment variable. */
static long interval_sec = 300;

static void *stats_flush_thread(void *arg) {
  while (true) {
    /* Current time (somewhere within some interval) */
    struct timeval now;
    struct timezone tz; /* Don't use this; it's deprecated */
    if (0 != gettimeofday(&now, &tz)) {
      fprintf(stderr, "warn: gettimeofday failed: %m\n");
      continue;
    }
    struct tm now_local;
    localtime_r(&now.tv_sec, &now_local);

    /* Begin/end of current interval */
    struct timeval begin = {
        .tv_sec = now.tv_sec - (now.tv_sec % interval_sec),
        .tv_usec = 0,
    };
    struct timeval end = {
        .tv_sec = begin.tv_sec + interval_sec,
        .tv_usec = 0,
    };

    /* Sleep until start of next interval */
    struct timeval delta = {
        .tv_sec = end.tv_sec - (now.tv_sec + 1),
        .tv_usec = 1000000 - now.tv_usec,
    };
    usleep(delta.tv_sec * 1000000 + delta.tv_usec);

    /* End previous interval, start new interval */
    stats_thread_submit_flush(begin, now_local);
  }
}

int spawn_stats_flush_thread(void) {
  int ret = 1; /* Error */

  /* Initialize the pthread attribute object */
  pthread_attr_t pthread_attr;
  errno = pthread_attr_init(&pthread_attr);
  if (0 != errno) {
    fprintf(stderr, "error: failed to initialize pthread_attr_t: %m\n");
    goto out_1;
  }

  /* Detached threads don't need to be joined. */
  errno = pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
  if (0 != errno) {
    fprintf(stderr, "error: failed to set thread detached state: %m\n");
    goto out_2;
  }

  /* Start thread that periodically inserts "flush" events */
  pthread_t tid;
  errno = pthread_create(&tid, &pthread_attr, stats_flush_thread, NULL);
  if (0 != errno) {
    fprintf(stderr, "error: failed to create thread: %m\n");
    goto out_2; /* bug: first thread is already started, so that leaks here */
  }

  ret = 0; /* success */

out_2:
  pthread_attr_destroy(&pthread_attr);
out_1:
  return ret;
}

int status_flush_thread_init(void) {
  char *interval_str;
  int interval;

  interval_str = getenv("INTERVAL");
  if (!interval_str)
    return 0;

  interval = atol(interval_str);
  if (!interval)
    return 0;

  if (interval < 1 || interval > 60 * 60 * 24) {
    fprintf(stderr, "error: bad interval: $d. must be between 1 and 86400.\n",
            interval);
    return 1;
  }
  if (interval < 60 ? 60 % interval != 0 : interval % 60 != 0) {
    fprintf(stderr, "error: bad interval: $d. must be a multiple or divisor of 60.\n",
            interval);
    return 1;
  }

  interval_sec = interval;

  return 0;
}