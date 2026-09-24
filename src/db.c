#include <todo/db.h>

#include <stdio.h>

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS todos ("
    "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  title        TEXT    NOT NULL,"
    "  priority     INTEGER NOT NULL DEFAULT 2,"
    "  done         INTEGER NOT NULL DEFAULT 0,"
    "  created_at   INTEGER NOT NULL,"
    "  completed_at INTEGER,"
    "  due          TEXT"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_todos_done ON todos(done);";

int todo_db_open(const char *path, sqlite3 **out)
{
    int rc = sqlite3_open_v2(path, out,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "打开数据库失败: %s\n", sqlite3_errmsg(*out));
        sqlite3_close(*out);
        *out = NULL;
        return -1;
    }
    sqlite3_busy_timeout(*out, 2000);
    return 0;
}

void todo_db_close(sqlite3 *db)
{
    if (db != NULL)
        sqlite3_close(db);
}

int todo_db_init(sqlite3 *db)
{
    char *err = NULL;
    if (sqlite3_exec(db, SCHEMA_SQL, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "初始化数据库失败: %s\n", err != NULL ? err : "未知错误");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}
