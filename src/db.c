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
    sqlite3_stmt *stmt = NULL;
    int version;

    /* Serialize migrations and enable referential integrity on every connection. */
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON; BEGIN IMMEDIATE;",
                     NULL, NULL, &err) != SQLITE_OK)
        goto fail;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, NULL) != SQLITE_OK)
        goto fail;
    if (sqlite3_step(stmt) != SQLITE_ROW)
        goto fail;
    version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    stmt = NULL;
    if (version > 2) {
        fprintf(stderr, "数据库版本 %d 高于本程序支持的版本，请升级 todo-cli\n", version);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    if (version < 2) {
        if (sqlite3_exec(db, SCHEMA_SQL, NULL, NULL, &err) != SQLITE_OK)
            goto fail;
        /* ALTER preserves task IDs and sqlite_sequence, including deleted IDs. */
        if (sqlite3_exec(db,
            "CREATE TABLE projects ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " name TEXT NOT NULL UNIQUE CHECK(length(trim(name)) > 0),"
            " created_at INTEGER NOT NULL);"
            "INSERT INTO projects(id, name, created_at) VALUES(1, 'TODO', strftime('%s','now'));"
            "ALTER TABLE todos ADD COLUMN project_id INTEGER REFERENCES projects(id);"
            "UPDATE todos SET project_id = 1;"
            "CREATE INDEX idx_todos_project ON todos(project_id, done, priority);"
            "PRAGMA user_version = 2;", NULL, NULL, &err) != SQLITE_OK)
            goto fail;
    }
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err) != SQLITE_OK)
        goto fail;
    return 0;

fail:
    fprintf(stderr, "初始化数据库失败: %s\n", err != NULL ? err : sqlite3_errmsg(db));
    sqlite3_free(err);
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    return -1;
}
