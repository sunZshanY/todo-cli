#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include <todo.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void cleanup_cli_args(TodoCliArgs *args)
{
    free(args->title);
    free(args->due);
    free(args->city);
    free(args->feeds);
    free(args->ids);
}

static int todo_main(int argc, char **argv)
{
    TodoCliArgs args;
    TodoConfig cfg;
    sqlite3 *db = NULL;
    int rc;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (todo_cli_parse(argc, argv, &args) != 0) {
        cleanup_cli_args(&args);
        return 2;
    }

    switch (args.cmd) {
    case CMD_HELP:
        todo_cli_help();
        return 0;
    case CMD_VERSION:
        printf("todo-cli %s (SQLite %s)\n", TODO_VERSION_STRING, sqlite3_libversion());
        return 0;
    default:
        break;
    }

    if (todo_config_load(&cfg) != 0) {
        fprintf(stderr, "加载配置失败\n");
        cleanup_cli_args(&args);
        return 1;
    }

    if (args.cmd == CMD_WEATHER) {
        rc = cmd_weather(&cfg, &args);
        cleanup_cli_args(&args);
        return rc;
    }
    if (args.cmd == CMD_NEWS) {
        rc = cmd_news(&cfg, &args);
        cleanup_cli_args(&args);
        return rc;
    }

    if (todo_mkdirs(cfg.data_dir) != 0) {
        fprintf(stderr, "无法创建数据目录: %s\n", cfg.data_dir);
        cleanup_cli_args(&args);
        return 1;
    }
    if (todo_db_open(cfg.db_path, &db) != 0) {
        cleanup_cli_args(&args);
        return 1;
    }
    if (todo_db_init(db) != 0) {
        todo_db_close(db);
        cleanup_cli_args(&args);
        return 1;
    }

    switch (args.cmd) {
    case CMD_ADD:
        rc = cmd_add(db, &args);
        break;
    case CMD_LIST:
        rc = cmd_list(db, &args);
        break;
    case CMD_DONE:
        rc = cmd_done(db, &args, 1);
        break;
    case CMD_UNDO:
        rc = cmd_done(db, &args, 0);
        break;
    case CMD_DEL:
        rc = cmd_delete(db, &args);
        break;
    case CMD_EDIT:
        rc = cmd_edit(db, &args);
        break;
    case CMD_CLEAR:
        rc = cmd_clear(db);
        break;
    case CMD_STATS:
        rc = cmd_stats(db);
        break;
    default:
        rc = 0;
        break;
    }

    todo_db_close(db);
    cleanup_cli_args(&args);
    return rc;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wargv)
{
    char **argv = NULL;
    int i, rc;

    argv = (char **)calloc((size_t)argc, sizeof(char *));
    if (argv == NULL)
        return 1;
    for (i = 0; i < argc; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        if (n <= 0) {
            rc = 1;
            goto done;
        }
        argv[i] = (char *)malloc((size_t)n);
        if (argv[i] == NULL) {
            rc = 1;
            goto done;
        }
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], n, NULL, NULL);
    }
    rc = todo_main(argc, argv);
done:
    if (argv != NULL) {
        for (i = 0; i < argc; i++)
            free(argv[i]);
        free(argv);
    }
    return rc;
}
#else
int main(int argc, char **argv)
{
    return todo_main(argc, argv);
}
#endif
