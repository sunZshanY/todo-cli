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
    case 0:
        return "P0";
    case 1:
        return "P1";
    case 3:
        return "P3";
    default:
        return "P2";
    }
}

int cmd_add(sqlite3 *db, const TodoCliArgs *args)
{
    long id = 0;

    if (todo_add_to_project(db, args->project, args->title,
                            (int)args->priority, args->due, &id) != 0)
        return 1;
    printf("已添加任务 #%ld [%s]: %s (优先级 %s)\n", id, args->project, args->title,
           priority_name((int)args->priority));
    return 0;
}

int cmd_list(sqlite3 *db, const TodoCliArgs *args)
{
    Todo **list = NULL;
    int count = 0, i, rc;

    if (args->project != NULL) {
        int exists = todo_project_exists(db, args->project);
        if (exists < 0)
            return 1;
        if (!exists) {
            fprintf(stderr, "项目不存在: %s\n", args->project);
            return 1;
        }
    }
    if (todo_list_filtered(db, args->project, args->list_filter,
                           (int)args->priority, &list, &count) != 0)
        return 1;
    if (count == 0) {
        printf("没有任务\n");
        return 0;
    }
    printf("状态 | 优先级 | ID | 项目 | 标题 | 截止日期\n");
    for (i = 0; i < count; i++) {
        Todo *t = list[i];
        printf("%s | %s | #%ld | %s | %s | %s\n",
               t->done ? "[x]" : "[ ]",
               priority_name(t->priority),
               t->id, t->project, t->title,
               t->due != NULL ? t->due : "-");
    }
    rc = 0;
    todo_free_list(list, count);
    return rc;
}

int cmd_project(sqlite3 *db, const TodoCliArgs *args)
{
    int exists, i, p;
    if (args->project_cmd == PROJECT_LIST) {
        TodoProject *projects = NULL;
        int count = 0;
        if (todo_project_list(db, &projects, &count) != 0)
            return 1;
        printf("Project | Todo | Done | Progress\n");
        for (i = 0; i < count; i++) {
            TodoProject *project = &projects[i];
            printf("%s | %d | %d | %d/%d\n", project->name,
                   project->total - project->done, project->done,
                   project->done, project->total);
        }
        todo_free_projects(projects, count);
        return 0;
    }
    exists = todo_project_exists(db, args->project);
    if (exists < 0)
        return 1;
    if (args->project_cmd == PROJECT_ADD) {
        if (exists) {
            fprintf(stderr, "项目已存在: %s\n", args->project);
            return 1;
        }
        if (todo_project_add(db, args->project) != 0)
            return 1;
        printf("已创建项目: %s\n", args->project);
        return 0;
    }
    if (!exists) {
        fprintf(stderr, "项目不存在: %s\n", args->project);
        return 1;
    }
    {
        Todo **list = NULL;
        int count = 0, done = 0;
        if (todo_list_filtered(db, args->project, TODO_FILTER_ALL, -1,
                               &list, &count) != 0)
            return 1;
        printf("%s\n────────────────────────────\n", args->project);
        for (p = 0; p <= 3; p++) {
            printf("\nP%d\n", p);
            for (i = 0; i < count; i++) {
                Todo *t = list[i];
                if (!t->done && t->priority == p) {
                    printf("  [ ] %s (#%ld)", t->title, t->id);
                    if (t->due != NULL)
                        printf("  due: %s", t->due);
                    putchar('\n');
                }
            }
        }
        printf("\nCompleted\n");
        for (i = 0; i < count; i++) {
            if (list[i]->done) {
                printf("  [x] %s (#%ld)\n", list[i]->title, list[i]->id);
                done++;
            }
        }
        printf("\nProgress: %d/%d\n", done, count);
        todo_free_list(list, count);
    }
    return 0;
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
