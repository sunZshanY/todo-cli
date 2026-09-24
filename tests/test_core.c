#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include <todo/db.h>
#include <todo/todo.h>
#include <todo/util.h>

static int checks = 0;
static int fails = 0;

#define CHECK(cond) \
    do { \
        checks++; \
        if (!(cond)) { \
            fails++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_LONG(a, b) \
    do { \
        long va_ = (a), vb_ = (b); \
        checks++; \
        if (va_ != vb_) { \
            fails++; \
            printf("FAIL %s:%d: %s == %s (%ld != %ld)\n", \
                   __FILE__, __LINE__, #a, #b, va_, vb_); \
        } \
    } while (0)

static void test_str_trim(void)
{
    char buf[64];
    strcpy(buf, "  hello world  ");
    CHECK(strcmp(todo_str_trim(buf), "hello world") == 0);
    strcpy(buf, "no-space");
    CHECK(strcmp(todo_str_trim(buf), "no-space") == 0);
}

static void test_str_to_long(void)
{
    long v = 0;
    CHECK(todo_str_to_long("42", &v) == 0 && v == 42);
    CHECK(todo_str_to_long("-7", &v) == 0 && v == -7);
    CHECK(todo_str_to_long("", &v) != 0);
    CHECK(todo_str_to_long("12x", &v) != 0);
    CHECK(todo_str_to_long("abc", &v) != 0);
}

static void test_starts_with(void)
{
    CHECK(todo_str_starts_with("hello", "he"));
    CHECK(todo_str_starts_with("hello", "hello"));
    CHECK(!todo_str_starts_with("hello", "help"));
    CHECK(!todo_str_starts_with("hi", "hello"));
}

static void test_date(void)
{
    CHECK(todo_is_valid_date("2026-10-01"));
    CHECK(todo_is_valid_date("1999-01-31"));
    CHECK(!todo_is_valid_date("2026-13-01"));
    CHECK(!todo_is_valid_date("2026-00-10"));
    CHECK(!todo_is_valid_date("2026-1-01"));
    CHECK(!todo_is_valid_date("2026/10/01"));
    CHECK(!todo_is_valid_date(""));
    CHECK(!todo_is_valid_date(NULL));
}

static void test_priority(void)
{
    CHECK(todo_is_valid_priority(1));
    CHECK(todo_is_valid_priority(2));
    CHECK(todo_is_valid_priority(3));
    CHECK(!todo_is_valid_priority(0));
    CHECK(!todo_is_valid_priority(4));
}

static void test_path_join(void)
{
    char buf[64];
    CHECK(todo_path_join("a", "b", buf, sizeof(buf)) == 0);
    CHECK(strcmp(buf, "a/b") == 0);
}

static void test_db_crud(void)
{
    sqlite3 *db = NULL;
    Todo **list = NULL;
    int count = 0, total = 0, done = 0, pending = 0, removed = 0;
    long id1 = 0, id2 = 0, id3 = 0;

    CHECK(todo_db_open(":memory:", &db) == 0);
    CHECK(db != NULL);
    CHECK(todo_db_init(db) == 0);

    CHECK(todo_add(db, "买牛奶", 1, "2026-10-01", &id1) == 0);
    CHECK(todo_add(db, "写周报", 2, NULL, &id2) == 0);
    CHECK(todo_add(db, "锻炼", 3, NULL, &id3) == 0);
    CHECK_LONG(id1, 1);
    CHECK_LONG(id2, 2);
    CHECK_LONG(id3, 3);

    CHECK(todo_stats(db, &total, &done, &pending) == 0);
    CHECK_LONG(total, 3);
    CHECK_LONG(done, 0);
    CHECK_LONG(pending, 3);

    CHECK(todo_exists(db, 1) == 1);
    CHECK(todo_exists(db, 99) == 0);

    CHECK(todo_list(db, TODO_FILTER_PENDING, &list, &count) == 0);
    CHECK_LONG(count, 3);
    CHECK_LONG(list[0]->id, id1);
    CHECK(strcmp(list[0]->title, "买牛奶") == 0);
    CHECK(strcmp(list[0]->due, "2026-10-01") == 0);
    CHECK_LONG(list[0]->priority, 1);
    todo_free_list(list, count);
    list = NULL;

    CHECK(todo_set_done(db, id1, 1) == 0);
    CHECK(todo_stats(db, &total, &done, &pending) == 0);
    CHECK_LONG(done, 1);
    CHECK_LONG(pending, 2);

    CHECK(todo_list(db, TODO_FILTER_DONE, &list, &count) == 0);
    CHECK_LONG(count, 1);
    CHECK_LONG(list[0]->id, id1);
    CHECK(list[0]->completed_at > 0);
    todo_free_list(list, count);
    list = NULL;

    CHECK(todo_update_title(db, id2, "写月报") == 0);
    CHECK(todo_list(db, TODO_FILTER_ALL, &list, &count) == 0);
    CHECK_LONG(count, 3);
    CHECK(strcmp(list[0]->title, "写月报") == 0);
    CHECK(strcmp(list[1]->title, "锻炼") == 0);
    CHECK_LONG(list[2]->id, id1);
    CHECK(list[2]->done == 1);
    todo_free_list(list, count);
    list = NULL;

    CHECK(todo_set_done(db, id1, 0) == 0);
    CHECK(todo_delete(db, id3) == 0);
    CHECK_LONG(todo_exists(db, id3), 0);

    CHECK(todo_set_done(db, id2, 1) == 0);
    CHECK(todo_clear_done(db, &removed) == 0);
    CHECK_LONG(removed, 1);
    CHECK(todo_stats(db, &total, &done, &pending) == 0);
    CHECK_LONG(total, 1);
    CHECK_LONG(pending, 1);

    todo_db_close(db);
}

static void test_mkdirs(void)
{
    char path[512];
    const char *tmp = NULL;
#ifdef _WIN32
    tmp = getenv("TEMP");
#else
    tmp = "/tmp";
#endif
    if (tmp == NULL)
        return;
    snprintf(path, sizeof(path), "%s/todo-cli-test/a/b/c", tmp);
    CHECK(todo_mkdirs(path) == 0);
    CHECK(todo_mkdirs(path) == 0);
}

int main(void)
{
    test_str_trim();
    test_str_to_long();
    test_starts_with();
    test_date();
    test_priority();
    test_path_join();
    test_db_crud();
    test_mkdirs();

    printf("%d 项检查，%d 失败\n", checks, fails);
    return fails > 0 ? 1 : 0;
}
