#ifndef QUA_JOURNAL_H
#define QUA_JOURNAL_H

#include <stdarg.h>

int journal_init(void);
void journal_add(const char *format, ...);
void journal_fini(void);

#endif