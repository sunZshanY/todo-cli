#include <todo/todo.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <todo/util.h>

#define TODO_ERR(expr, db) \
    do { \
        fprintf(stderr, "数据库错误: %s\n", sqlite3_errmsg(db)); \
        (void)(expr); \
    } while (0)

int todo_add(sqlite3 *db, const char *title, int priority, const char *due,
             long *new_id)
{
    sqlite3_stmt *stmt = NULL;
    long long now = todo_time_now();
    int rc;

    rc = sqlite3_prepare_v2(db,
        "INSERT INTO todos(title, priority, done, created_at, due) "
        "VALUES(?1, ?2, 0, ?3, ?4)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        TODO_ERR(0, db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, priority);
    sqlite3_bind_int64(stmt, 3, now);
    if (due != NULL && *due != '\0')
        sqlite3_bind_text(stmt, 4, due, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 4);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        TODO_ERR(0, db);
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    if (new_id != NULL)
        *new_id = (long)sqlite3_last_insert_rowid(db);
    return 0;
}

int todo_list(sqlite3 *db, int filter, Todo ***out, int *count)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql;
    Todo **list = NULL;
    int n = 0, cap = 0;
    int rc;

    switch (filter) {
    case TODO_FILTER_DONE:
        sql = "SELECT id, title, priority, done, created_at, completed_at, due "
              "FROM todos WHERE done = 1 ORDER BY completed_at DESC, id DESC";
        break;
    case TODO_FILTER_ALL:
        sql = "SELECT id, title, priority, done, created_at, completed_at, due "
              "FROM todos ORDER BY done ASC, priority ASC, due IS NULL, due ASC, id ASC";
        break;
    default:
        sql = "SELECT id, title, priority, done, created_at, completed_at, due "
              "FROM todos WHERE done = 0 ORDER BY priority ASC, due IS NULL, due ASC, id ASC";
        break;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        TODO_ERR(0, db);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Todo *t;
        if (n >= cap) {
            Todo **grown;
            cap = cap == 0 ? 16 : cap * 2;
            grown = (Todo **)realloc(list, (size_t)cap * sizeof(Todo *));
            if (grown == NULL)
                goto oom;
            list = grown;
        }
        t = (Todo *)calloc(1, sizeof(Todo));
        if (t == NULL)
            goto oom;
        t->id = (long)sqlite3_column_int64(stmt, 0);
        t->title = todo_strdup((const char *)sqlite3_column_text(stmt, 1));
        t->priority = sqlite3_column_int(stmt, 2);
        t->done = sqlite3_column_int(stmt, 3);
        t->created_at = sqlite3_column_int64(stmt, 4);
        t->completed_at = sqlite3_column_int64(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL)
            t->due = todo_strdup((const char *)sqlite3_column_text(stmt, 6));
        if (t->title == NULL || (sqlite3_column_type(stmt, 6) != SQLITE_NULL && t->due == NULL))
            goto oom;
        list[n++] = t;
    }
    if (rc != SQLITE_DONE) {
        TODO_ERR(0, db);
        sqlite3_finalize(stmt);
        todo_free_list(list, n);
        return -1;
    }
    sqlite3_finalize(stmt);
    *out = list;
    *count = n;
    return 0;

oom:
    fprintf(stderr, "内存不足\n");
    if (stmt != NULL)
        sqlite3_finalize(stmt);
    todo_free_list(list, n);
    return -1;
}

int todo_set_done(sqlite3 *db, long id, int done)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (done) {
        rc = sqlite3_prepare_v2(db,
            "UPDATE todos SET done = 1, completed_at = ?1 WHERE id = ?2",
            -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            TODO_ERR(0, db);
            return -1;
        }
        sqlite3_bind_int64(stmt, 1, todo_time_now());
        sqlite3_bind_int64(stmt, 2, id);
    } else {
        rc = sqlite3_prepare_v2(db,
            "UPDATE todos SET done = 0, completed_at = NULL WHERE id = ?1",
            -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            TODO_ERR(0, db);
            return -1;
        }
        sqlite3_bind_int64(stmt, 1, id);
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        TODO_ERR(0, db);
        return -1;
    }
    return 0;
}

int todo_update_title(sqlite3 *db, long id, const char *title)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db, "UPDATE todos SET title = ?1 WHERE id = ?2",
                            -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        TODO_ERR(0, db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        TODO_ERR(0, db);
        return -1;
    }
    return 0;
}

int todo_delete(sqlite3 *db, long id)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db, "DELETE FROM todos WHERE id = ?1", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        TODO_ERR(0, db);
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        TODO_ERR(0, db);
        return -1;
    }
    return 0;
}

int todo_clear_done(sqlite3 *db, int *removed)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, "DELETE FROM todos WHERE done = 1", NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "数据库错误: %s\n", err != NULL ? err : "未知错误");
        sqlite3_free(err);
        return -1;
    }
    if (removed != NULL)
        *removed = sqlite3_changes(db);
    return 0;
}

int todo_stats(sqlite3 *db, int *total, int *done, int *pending)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db,
        "SELECT COUNT(*),"
        " SUM(CASE WHEN done = 1 THEN 1 ELSE 0 END),"
        " SUM(CASE WHEN done = 0 THEN 1 ELSE 0 END)"
        " FROM todos",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        TODO_ERR(0, db);
        return -1;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        TODO_ERR(0, db);
        sqlite3_finalize(stmt);
        return -1;
    }
    *total = sqlite3_column_int(stmt, 0);
    *done = sqlite3_column_int(stmt, 1);
    *pending = sqlite3_column_int(stmt, 2);
    sqlite3_finalize(stmt);
    return 0;
}

int todo_exists(sqlite3 *db, long id)
{
    sqlite3_stmt *stmt = NULL;
    int rc, exists = 0;

    rc = sqlite3_prepare_v2(db, "SELECT 1 FROM todos WHERE id = ?1", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        TODO_ERR(0, db);
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
        exists = 1;
    sqlite3_finalize(stmt);
    return exists;
}

void todo_free_list(Todo **list, int count)
{
    int i;
    if (list == NULL)
        return;
    for (i = 0; i < count; i++) {
        if (list[i] != NULL) {
            free(list[i]->title);
            free(list[i]->due);
            free(list[i]);
        }
    }
    free(list);
}
