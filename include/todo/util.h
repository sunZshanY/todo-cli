#ifndef TODO_UTIL_H
#define TODO_UTIL_H

#include <stddef.h>

char *todo_strdup(const char *s);
char *todo_str_trim(char *s);
int todo_str_to_long(const char *s, long *out);
int todo_str_starts_with(const char *s, const char *prefix);
int todo_mkdirs(const char *path);
int todo_path_join(const char *dir, const char *name, char *buf, size_t cap);
long long todo_time_now(void);
int todo_is_valid_date(const char *s);
int todo_is_valid_priority(long p);
int todo_exe_dir(char *buf, size_t cap);
int todo_file_exists(const char *path);

#endif
