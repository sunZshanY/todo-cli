#include <todo/commands.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <todo/ext.h>
#include <todo/todo.h>
#include <todo/util.h>

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

static void local_time(struct tm *out, long long ts)
{
    time_t t = (time_t)ts;
    memset(out, 0, sizeof(*out));
#ifdef _WIN32
    if (localtime_s(out, &t) != 0)
#else
    if (localtime_r(&t, out) == NULL)
#endif
        out->tm_year = 0;
}

static void format_datetime(char *buf, size_t cap, long long ts)
{
    struct tm tm_val;
    local_time(&tm_val, ts);
    if (tm_val.tm_year == 0) {
        snprintf(buf, cap, "%lld", ts);
        return;
    }
    snprintf(buf, cap, "%04d-%02d-%02d %02d:%02d",
             tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
             tm_val.tm_hour, tm_val.tm_min);
}

static int days_in_month(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month == 2 && leap)
        return 29;
    return days[month - 1];
}

/* Monday-first weekday index: 0 = Monday ... 6 = Sunday. */
static int first_weekday(int year, int month)
{
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = 1;
    if (mktime(&tm_val) == (time_t)-1)
        return 0;
    return (tm_val.tm_wday + 6) % 7;
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
        printf("已删除任务 #%ld（已移入回收站，可用 todo restore 恢复）\n", args->ids[i]);
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
    if (todo_edit(db, args->ids[0], args->title, (int)args->priority, args->due) != 0)
        return 1;
    printf("已更新任务 #%ld", args->ids[0]);
    if (args->title != NULL)
        printf(" 标题: %s", args->title);
    if (args->priority >= 0)
        printf(" 优先级: %s", priority_name((int)args->priority));
    if (args->due != NULL)
        printf(" 截止日期: %s", args->due);
    putchar('\n');
    return 0;
}

int cmd_clear(sqlite3 *db)
{
    int removed = 0;

    if (todo_clear_done(db, &removed) != 0)
        return 1;
    printf("已清除 %d 条已完成任务（已移入回收站）\n", removed);
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

int cmd_trash(sqlite3 *db, const TodoCliArgs *args)
{
    TrashItem **list = NULL;
    int count = 0, i;

    (void)args;
    if (todo_trash_list(db, &list, &count) != 0)
        return 1;
    if (count == 0) {
        printf("回收站为空\n");
        return 0;
    }
    printf("状态 | 优先级 | 回收ID | 原ID | 项目 | 标题 | 截止日期 | 删除时间\n");
    for (i = 0; i < count; i++) {
        TrashItem *t = list[i];
        char deleted[32];
        format_datetime(deleted, sizeof(deleted), t->deleted_at);
        printf("%s | %s | #%ld | #%ld | %s | %s | %s | %s\n",
               t->done ? "[x]" : "[ ]",
               priority_name(t->priority),
               t->id, t->original_id,
               t->project != NULL ? t->project : "?",
               t->title,
               t->due != NULL ? t->due : "-",
               deleted);
    }
    todo_free_trash(list, count);
    return 0;
}

int cmd_restore(sqlite3 *db, const TodoCliArgs *args)
{
    int i, fails = 0;

    for (i = 0; i < args->id_count; i++) {
        long restored_id = 0;
        int exists = todo_trash_exists(db, args->ids[i]);
        if (exists < 0)
            return 1;
        if (exists == 0) {
            fprintf(stderr, "回收站任务 #%ld 不存在\n", args->ids[i]);
            fails++;
            continue;
        }
        if (todo_trash_restore(db, args->ids[i], &restored_id) != 0)
            return 1;
        printf("已恢复任务 #%ld（原任务 ID #%ld）\n", args->ids[i], restored_id);
    }
    return fails > 0 ? 1 : 0;
}

int cmd_purge(sqlite3 *db, const TodoCliArgs *args)
{
    int i, fails = 0, removed = 0;

    if (args->purge_all) {
        if (todo_trash_clear(db, &removed) != 0)
            return 1;
        printf("已清空回收站，永久删除 %d 条任务\n", removed);
        return 0;
    }
    for (i = 0; i < args->id_count; i++) {
        int exists = todo_trash_exists(db, args->ids[i]);
        if (exists < 0)
            return 1;
        if (exists == 0) {
            fprintf(stderr, "回收站任务 #%ld 不存在\n", args->ids[i]);
            fails++;
            continue;
        }
        if (todo_trash_delete(db, args->ids[i]) != 0)
            return 1;
        printf("已永久删除回收站任务 #%ld\n", args->ids[i]);
    }
    return fails > 0 ? 1 : 0;
}

int cmd_cal(sqlite3 *db, const TodoCliArgs *args)
{
    Todo **list = NULL;
    int count = 0, i, day, year, month, offset, dim;
    int has_due[32] = {0};
    char prefix[8];
    struct tm now_tm;

    if (args->cal_set) {
        year = args->cal_year;
        month = args->cal_month;
    } else {
        long long now = todo_time_now();
        local_time(&now_tm, now);
        if (now_tm.tm_year == 0) {
            fprintf(stderr, "无法获取当前日期\n");
            return 1;
        }
        year = now_tm.tm_year + 1900;
        month = now_tm.tm_mon + 1;
    }
    snprintf(prefix, sizeof(prefix), "%04d-%02d", year, month);

    if (todo_calendar(db, prefix, &list, &count) != 0)
        return 1;
    for (i = 0; i < count; i++) {
        if (list[i]->due != NULL && strlen(list[i]->due) == 10)
            has_due[atoi(list[i]->due + 8)] = 1;
    }

    printf("%d年%d月\n", year, month);
    printf(" %5s %5s %5s %5s %5s %5s %5s\n",
           "一", "二", "三", "四", "五", "六", "日");
    local_time(&now_tm, todo_time_now());
    offset = first_weekday(year, month);
    for (i = 0; i < offset; i++)
        printf(" %5s", "");
    dim = days_in_month(year, month);
    for (day = 1; day <= dim; day++) {
        char cell[8];
        char marker = has_due[day] ? '*' : ' ';
        if (now_tm.tm_year + 1900 == year && now_tm.tm_mon + 1 == month &&
            now_tm.tm_mday == day)
            snprintf(cell, sizeof(cell), "[%2d]%c", day, marker);
        else
            snprintf(cell, sizeof(cell), "%2d%c", day, marker);
        printf(" %5s", cell);
        if ((offset + day) % 7 == 0)
            putchar('\n');
    }
    if ((offset + dim) % 7 != 0)
        putchar('\n');

    printf("\n提醒事项（%s）:\n", prefix);
    if (count == 0) {
        printf("本月没有提醒事项\n");
        todo_free_list(list, count);
        return 0;
    }
    for (i = 0; i < count; i++) {
        Todo *t = list[i];
        printf("%s | %s | %s | #%ld | %s | %s\n",
               t->due, t->done ? "[x]" : "[ ]",
               priority_name(t->priority),
               t->id, t->project, t->title);
    }
    printf("共 %d 条提醒（* 表示当天有提醒）\n", count);
    todo_free_list(list, count);
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
