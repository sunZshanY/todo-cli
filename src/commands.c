#include <todo/commands.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <todo/ext.h>
#include <todo/todo.h>

#define EXT_BUFFER_CAP 65536

static const char *priority_name(int p)
{
    switch (p) {
    case 1:
        return "高";
    case 3:
        return "低";
    default:
        return "中";
    }
}

int cmd_add(sqlite3 *db, const TodoCliArgs *args)
{
    long id = 0;

    if (todo_add(db, args->title, (int)args->priority, args->due, &id) != 0)
        return 1;
    printf("已添加任务 #%ld: %s (优先级 %s)\n", id, args->title,
           priority_name((int)args->priority));
    return 0;
}

int cmd_list(sqlite3 *db, const TodoCliArgs *args)
{
    Todo **list = NULL;
    int count = 0, i, rc;

    if (todo_list(db, args->list_filter, &list, &count) != 0)
        return 1;
    if (count == 0) {
        printf("没有任务\n");
        return 0;
    }
    printf("%-3s %-5s %-12s %-4s %s\n", "状态", "优先级", "截止日期", "ID", "标题");
    printf("%-3s %-5s %-12s %-4s %s\n", "----", "------", "----------", "--", "----");
    for (i = 0; i < count; i++) {
        Todo *t = list[i];
        printf("%-3s %-5s %-12s %-4ld %s\n",
               t->done ? "[x]" : "[ ]",
               priority_name(t->priority),
               t->due != NULL ? t->due : "-",
               t->id, t->title);
    }
    rc = 0;
    todo_free_list(list, count);
    return rc;
}

int cmd_done(sqlite3 *db, const TodoCliArgs *args, int done)
{
    int i, fails = 0;

    for (i = 0; i < args->id_count; i++) {
        int exists = todo_exists(db, args->ids[i]);
        if (exists < 0)
            return 1;
        if (exists == 0) {
            fprintf(stderr, "任务 #%ld 不存在\n", args->ids[i]);
            fails++;
            continue;
        }
        if (todo_set_done(db, args->ids[i], done) != 0)
            return 1;
        printf("%s #%ld\n", done ? "已完成任务" : "已取消完成任务", args->ids[i]);
    }
    return fails > 0 ? 1 : 0;
}

int cmd_delete(sqlite3 *db, const TodoCliArgs *args)
{
    int i, fails = 0;

    for (i = 0; i < args->id_count; i++) {
        int exists = todo_exists(db, args->ids[i]);
        if (exists < 0)
            return 1;
        if (exists == 0) {
            fprintf(stderr, "任务 #%ld 不存在\n", args->ids[i]);
            fails++;
            continue;
        }
        if (todo_delete(db, args->ids[i]) != 0)
            return 1;
        printf("已删除任务 #%ld\n", args->ids[i]);
    }
    return fails > 0 ? 1 : 0;
}

int cmd_edit(sqlite3 *db, const TodoCliArgs *args)
{
    int exists = todo_exists(db, args->ids[0]);
    if (exists < 0)
        return 1;
    if (exists == 0) {
        fprintf(stderr, "任务 #%ld 不存在\n", args->ids[0]);
        return 1;
    }
    if (todo_update_title(db, args->ids[0], args->title) != 0)
        return 1;
    printf("已更新任务 #%ld: %s\n", args->ids[0], args->title);
    return 0;
}

int cmd_clear(sqlite3 *db)
{
    int removed = 0;

    if (todo_clear_done(db, &removed) != 0)
        return 1;
    printf("已清除 %d 条已完成任务\n", removed);
    return 0;
}

int cmd_stats(sqlite3 *db)
{
    int total = 0, done = 0, pending = 0;

    if (todo_stats(db, &total, &done, &pending) != 0)
        return 1;
    printf("总计 %d | 已完成 %d | 待办 %d\n", total, done, pending);
    return 0;
}

static int run_ext(const TodoConfig *cfg, const char *script,
                   const char *const *args, int arg_count)
{
    char buf[EXT_BUFFER_CAP];
    int exit_code = 0;

    if (todo_ext_run_python(cfg, script, args, arg_count,
                            buf, sizeof(buf), &exit_code) != 0) {
        fprintf(stderr, "无法启动 Python，请确认已安装 Python 3 且在 PATH 中，"
                        "或在配置文件中设置 python 路径\n");
        return 1;
    }
    if (buf[0] != '\0')
        fputs(buf, stdout);
    if (exit_code == 127) {
        fprintf(stderr, "未找到 Python 解释器 (%s)，请安装 Python 3 或修改配置\n",
                cfg->python);
        return 1;
    }
    if (exit_code != 0)
        fprintf(stderr, "脚本执行失败 (退出码 %d)\n", exit_code);
    return exit_code == 0 ? 0 : 1;
}

int cmd_weather(const TodoConfig *cfg, const TodoCliArgs *args)
{
    const char *city = NULL;
    const char *argv_ext[1];
    int arg_count = 0;

    if (args->city != NULL && *args->city != '\0') {
        city = args->city;
    } else if (cfg->default_city[0] != '\0') {
        city = cfg->default_city;
    }
    if (city != NULL) {
        argv_ext[0] = city;
        arg_count = 1;
    }
    return run_ext(cfg, "weather.py", argv_ext, arg_count);
}

int cmd_news(const TodoConfig *cfg, const TodoCliArgs *args)
{
    const char *feeds = NULL;
    const char *argv_ext[1];
    int arg_count = 0;

    if (args->feeds != NULL && *args->feeds != '\0')
        feeds = args->feeds;
    else if (cfg->news_feeds[0] != '\0')
        feeds = cfg->news_feeds;
    if (feeds != NULL) {
        argv_ext[0] = feeds;
        arg_count = 1;
    }
    return run_ext(cfg, "news.py", argv_ext, arg_count);
}
