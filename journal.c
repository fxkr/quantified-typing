#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "journal.h"

static FILE *journal_stream;

int journal_init(void) {
  char *dir = getenv("LOGS_DIRECTORY");

  char path[PATH_MAX];

  if (!dir || 0 == strcmp(dir, "")) {
    journal_stream = stderr;
    return 0;
  }

  if (snprintf(path, sizeof(path), "%s/typing.log", dir) >=
      sizeof(path)) {
    fprintf(stderr, "error: failed to build journal file path\n");
    return 1;
  }

  journal_stream = fopen(path, "a+");
  if (!journal_stream) {
    fprintf(stderr, "error: failed to open journal file %s: %m\n", path);
    return 1;
  }

  return 0;
}

void __attribute__((format(printf, 1, 2)))
journal_add(const char *format, ...) {
  va_list args;
  va_start(args, format);

  vfprintf(journal_stream, format, args);
  fflush(journal_stream);

  va_end(args);
}

void journal_fini() {}