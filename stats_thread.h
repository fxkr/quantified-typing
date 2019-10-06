#ifndef QUA_STATS_THREAD_H
#define QUA_STATS_THREAD_H

#include <sys/time.h>
#include <time.h>

int spawn_stats_thread(void);

int stats_thread_submit_key(struct timespec *wall, struct timespec *delta);

int stats_thread_submit_flush(struct timeval start_time,
                              struct tm start_time_local);

#endif
