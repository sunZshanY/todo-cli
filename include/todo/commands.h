#ifndef TODO_COMMANDS_H
#define TODO_COMMANDS_H

#include <sqlite3.h>

#include "cli.h"
#include "config.h"

int cmd_add(sqlite3 *db, const TodoCliArgs *args);
int cmd_list(sqlite3 *db, const TodoCliArgs *args);
int cmd_done(sqlite3 *db, const TodoCliArgs *args, int done);
int cmd_delete(sqlite3 *db, const TodoCliArgs *args);
int cmd_edit(sqlite3 *db, const TodoCliArgs *args);
int cmd_clear(sqlite3 *db);
int cmd_stats(sqlite3 *db);
int cmd_weather(const TodoConfig *cfg, const TodoCliArgs *args);
int cmd_news(const TodoConfig *cfg, const TodoCliArgs *args);

#endif
