#ifndef TODO_EXT_H
#define TODO_EXT_H

#include <stddef.h>

#include "config.h"

int todo_ext_run_python(const TodoConfig *cfg, const char *script,
                        const char *const *args, int arg_count,
                        char *out, size_t cap, int *exit_code);

#endif
