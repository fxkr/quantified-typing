#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep // required for abort
#include <sys/queue.h>

#include "stats_thread.h"

int const max_bucket_ms = 2000;
int const bucket_width_ms = 10;
int const num_regular_buckets = max_bucket_ms / bucket_width_ms;
int const num_buckets = num_regular_buckets + 1; /* overflow bucket */

enum stats_thread_event_type {
  STATS_THREAD_EVENT_TYPE_KEY,
  STATS_THREAD_EVENT_TYPE_FLUSH,
};

struct stats_thread_event {
  TAILQ_ENTRY(stats_thread_event) entries;

  enum stats_thread_event_type type;

  union {
    struct {
      int ms;
    } key;
    struct {
      struct timeval start_time;
      struct tm start_time_local;
    } flush;
  } value;
};

static struct {
  /* buckets is the distribution of delays between keypresses in the current
   * interval */
  int bucket[num_buckets];

  /* num_keys is the total number of keys pressed in the current interval,
   * across all buckets. */
  int num_keys;

  /* queue_head holds events send to this thread. New events go at the end. */
  TAILQ_HEAD(tailhead, stats_thread_event) queue_head;

  /* queue_mutex needs to be acquired for any access (read or write) to the
   * queue. */
  pthread_mutex_t queue_mutex;

  /* queue_cond is signaled whenever an event has been added to the queue.*/
  pthread_cond_t queue_cond;
} stats_thread_data;

int stats_thread_submit_key(struct timespec *wall, struct timespec *delta) {

  struct stats_thread_event *e = calloc(sizeof(struct stats_thread_event), 1);
  if (!e)
    return 1;

  e->type = STATS_THREAD_EVENT_TYPE_KEY;
  e->value.key.ms = delta->tv_sec * 1000 + delta->tv_nsec / 1000000;

  pthread_mutex_lock(&stats_thread_data.queue_mutex);
  TAILQ_INSERT_TAIL(&stats_thread_data.queue_head, e, entries);
  pthread_mutex_unlock(&stats_thread_data.queue_mutex);
  pthread_cond_signal(&stats_thread_data.queue_cond);

  return 0;
}

int stats_thread_submit_flush(struct timeval start_time,
                              struct tm start_time_local) {
  struct stats_thread_event *e = calloc(sizeof(struct stats_thread_event), 1);
  if (!e)
    return 1;

  e->type = STATS_THREAD_EVENT_TYPE_FLUSH;
  e->value.flush.start_time = start_time;
  e->value.flush.start_time_local = start_time_local;

  pthread_mutex_lock(&stats_thread_data.queue_mutex);
  TAILQ_INSERT_TAIL(&stats_thread_data.queue_head, e, entries);
  pthread_mutex_unlock(&stats_thread_data.queue_mutex);
  pthread_cond_signal(&stats_thread_data.queue_cond);

  return 0;
}

static int bucket_index_from_msec(int msec) {
  if (msec < 0)
    return 0;
  if (msec > max_bucket_ms)
    return num_regular_buckets; /* overflow bucket */
  return msec / bucket_width_ms;
}

static int bucket_index_to_bucket_name(int idx, char *out, size_t out_len) {
  if (idx >= num_regular_buckets)
    return snprintf(out, out_len, "inf");
  return snprintf(out, out_len, "%d", bucket_width_ms * idx);
}

static void bucket_add_msec(int msec) {
  stats_thread_data.bucket[bucket_index_from_msec(msec)]++;
}

static void stats_thread_flush(struct timeval *start_time,
                               struct tm *start_time_local) {

  char start_time_local_str[32];
  strftime(start_time_local_str, sizeof(start_time_local_str),
           "%Y-%m-%d %H:%M:%S", start_time_local);

  printf("{\"t\":\"%ld\",\"tz\":\"%s\",\"events\":{", start_time->tv_sec,
         start_time_local_str);

  bool not_first = false;
  for (int i = 0; i < num_buckets; i++) {
    if (stats_thread_data.bucket[i] <= 0)
      continue;
    if (not_first)
      printf(",");
    else
      not_first = true;

    char bucket_name[32];
    bucket_index_to_bucket_name(i, &bucket_name, sizeof(bucket_name));
    printf("\"%s\":%d", bucket_name, stats_thread_data.bucket[i]);
  }

  printf("}}\n");
}

static void stats_thread_reset_buckets(void) {
  for (int i = 0; i < num_buckets; i++) {
    stats_thread_data.bucket[i] = 0;
  }
}

static void *stats_thread(void *arg) {
  int ret;

  while (true) {

    /* wait for element in queue (after this we hold .queue_mutex) */
    do {
      ret = pthread_cond_wait(&stats_thread_data.queue_cond,
                              &stats_thread_data.queue_mutex);
      if (stats_thread_data.queue_head.tqh_first) {
        break; /* element available, we're running, and we have the mutex */
      }
      pthread_mutex_unlock(&stats_thread_data.queue_mutex);
    } while (true);

    /* process all elements */
    while (stats_thread_data.queue_head.tqh_first != NULL) {
      struct stats_thread_event *e = stats_thread_data.queue_head.tqh_first;

      TAILQ_REMOVE(&stats_thread_data.queue_head,
                   stats_thread_data.queue_head.tqh_first, entries);

      switch (e->type) {
      case STATS_THREAD_EVENT_TYPE_KEY:
        bucket_add_msec(e->value.key.ms);
        break;

      case STATS_THREAD_EVENT_TYPE_FLUSH:
        pthread_mutex_unlock(&stats_thread_data.queue_mutex);
        stats_thread_flush(&e->value.flush.start_time,
                           &e->value.flush.start_time_local);
        stats_thread_reset_buckets();
        pthread_mutex_lock(&stats_thread_data.queue_mutex);
        break;
      default:
        break;
      }

      free(e);
    }

    pthread_mutex_unlock(&stats_thread_data.queue_mutex);
  }

  return NULL;
}

int spawn_stats_thread(void) {
  int ret = 1; /* Error */

  /* Initialize thread data */
  TAILQ_INIT(&stats_thread_data.queue_head);
  pthread_mutex_init(&stats_thread_data.queue_mutex, NULL);
  pthread_cond_init(&stats_thread_data.queue_cond, NULL);

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

  /* Start thread that processes events from the queue */
  pthread_t tid;
  errno = pthread_create(&tid, &pthread_attr, stats_thread, NULL);
  if (0 != errno) {
    fprintf(stderr, "error: failed to create thread: %m\n");
    goto out_2;
  }

  ret = 0; /* success */

out_2:
  pthread_attr_destroy(&pthread_attr);
out_1:
  return ret;
}
