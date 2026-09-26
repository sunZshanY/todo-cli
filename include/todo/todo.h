#ifndef TODO_TODO_H
#define TODO_TODO_H

#include <sqlite3.h>

typedef struct {
    long id;
    char *title;
    int priority;
    int done;
    long long created_at;
    long long completed_at;
    char *due;
} Todo;

enum {
    TODO_FILTER_PENDING = 0,
    TODO_FILTER_DONE = 1,
    TODO_FILTER_ALL = 2
};

int todo_add(sqlite3 *db, const char *title, int priority, const char *due,
             long *new_id);
int todo_list(sqlite3 *db, int filter, Todo ***out, int *count);
int todo_set_done(sqlite3 *db, long id, int done);
int todo_update_title(sqlite3 *db, long id, const char *title);
int todo_delete(sqlite3 *db, long id);
int todo_clear_done(sqlite3 *db, int *removed);
int todo_stats(sqlite3 *db, int *total, int *done, int *pending);
int todo_exists(sqlite3 *db, long id);
void todo_free_list(Todo **list, int count);

#endif
