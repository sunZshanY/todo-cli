#ifndef TODO_DB_H
#define TODO_DB_H

#include <sqlite3.h>

int todo_db_open(const char *path, sqlite3 **out);
void todo_db_close(sqlite3 *db);
int todo_db_init(sqlite3 *db);

#endif
