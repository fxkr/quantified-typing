#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "dev_input_set.h"

pthread_mutex_t lock;

LIST_HEAD(listhead, entry) head;

struct head *headp;

struct entry {
  LIST_ENTRY(entry) entries;

  char *name;
} * dev_list;

/* Not thread-safe! */
void dev_input_set_init(void) { LIST_INIT(&head); }

/*
 * Add string to set.
 * If already present, do nothing.
 * Return 0 if it was newly added, 1 otherwise.
 * Thread-safe.
 * Can wait on mutex.
 * Operation is O(n) - set is assumed to be small!
 */
int dev_input_set_add(char *name) {
  int rc = 0;

  struct entry *e = calloc(sizeof(*e), 1);
  if (!e) {
    fprintf(stderr, "error: %m\n");
    goto err_1;
  }

  e->name = strdup(name);
  if (!e->name) {
    fprintf(stderr, "error: %m\n");
    goto err_2;
  }

  pthread_mutex_lock(&lock);

  struct entry *np;
  bool was_already_present = false;
  for (np = head.lh_first; np != NULL; np = np->entries.le_next) {
    if (0 == strcmp(e->name, np->name)) {
      was_already_present = true;
      break;
    }
  }

  if (!was_already_present) {
    LIST_INSERT_HEAD(&head, e, entries);
  }

  pthread_mutex_unlock(&lock);

  if (was_already_present) {
    goto err_3;
  }

  return 0; /* Success */

err_3:
  free(e->name);
err_2:
  free(e);
err_1:
  return 1;
}

/*
 * Remove string from set.
 * If not present, do nothing.
 * Thread-safe.
 * Can wait on mutex.
 * Operation is O(n) - set is assumed to be small!
 */
void dev_input_set_remove(char *name) {
  pthread_mutex_lock(&lock);

  struct entry *np;
  for (np = head.lh_first; np != NULL; np = np->entries.le_next) {
    if (0 == strcmp(name, np->name)) {
      LIST_REMOVE(np, entries);
      free(np->name);
      free(np);
      break;
    }
  }

  pthread_mutex_unlock(&lock);
}
