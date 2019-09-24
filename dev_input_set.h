#ifndef QUA_DEV_INPUT_SET_H
#define QUA_DEV_INPUT_SET_H

/* dev_input_set_init must be called exactly once before using related methods. Returns 1 on error. */
void dev_input_set_init(void);

/* dev_input_set_add adds name to set. If already present, does nothing. Returns 0 if added, 1 otherwise (already present, or malloc failed). Thread-safe. */
int dev_input_set_add(char *name);

/* dev_input_set_remove removes name from set. If not present, does nothing. Thread-safe. */
void dev_input_set_remove(char *name);

#endif
