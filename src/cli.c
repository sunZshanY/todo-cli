#include <todo/cli.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <todo/util.h>

static char *join_parts(const char *const *parts, int count, const char *sep)
{
    int i;
    size_t total = 1;
    char *buf, *p;

    if (count <= 0)
        return NULL;
    for (i = 0; i < count; i++)
        total += strlen(parts[i]) + strlen(sep);
    buf = (char *)malloc(total);
    if (buf == NULL)
        return NULL;
    p = buf;
    for (i = 0; i < count; i++) {
        size_t n = strlen(parts[i]);
        if (i > 0) {
            memcpy(p, sep, strlen(sep));
            p += strlen(sep);
        }
        memcpy(p, parts[i], n);
        p += n;
    }
    *p = '\0';
    return buf;
}

static TodoCommand match_command(const char *s)
{
    if (strcmp(s, "add") == 0 || strcmp(s, "a") == 0)
        return CMD_ADD;
    if (strcmp(s, "list") == 0 || strcmp(s, "ls") == 0)
        return CMD_LIST;
    if (strcmp(s, "done") == 0)
        return CMD_DONE;
    if (strcmp(s, "undo") == 0)
        return CMD_UNDO;
    if (strcmp(s, "del") == 0 || strcmp(s, "rm") == 0 || strcmp(s, "delete") == 0)
        return CMD_DEL;
    if (strcmp(s, "edit") == 0)
        return CMD_EDIT;
    if (strcmp(s, "clear") == 0)
        return CMD_CLEAR;
    if (strcmp(s, "stats") == 0)
        return CMD_STATS;
    if (strcmp(s, "weather") == 0 || strcmp(s, "w") == 0)
        return CMD_WEATHER;
    if (strcmp(s, "news") == 0 || strcmp(s, "n") == 0)
        return CMD_NEWS;
    if (strcmp(s, "help") == 0 || strcmp(s, "-h") == 0 || strcmp(s, "--help") == 0)
        return CMD_HELP;
    if (strcmp(s, "version") == 0 || strcmp(s, "-v") == 0 || strcmp(s, "--version") == 0)
        return CMD_VERSION;
    return CMD_NONE;
}

static int parse_add(int argc, char **argv, TodoCliArgs *out)
{
    int i;
    int title_count = 0;
    const char **parts;
    long v;

    out->priority = 2;
    parts = (const char **)calloc((size_t)(argc > 0 ? argc : 1), sizeof(char *));
    if (parts == NULL)
        return -1;

    for (i = 0; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            for (i++; i < argc; i++)
                parts[title_count++] = argv[i];
            break;
        }
        if (strcmp(arg, "-p") == 0 || strcmp(arg, "--priority") == 0) {
            if (i + 1 >= argc || todo_str_to_long(argv[++i], &v) != 0 ||
                !todo_is_valid_priority(v)) {
                fprintf(stderr, "无效的优先级，应为 1(高) 2(中) 3(低)\n");
                free(parts);
                return -1;
            }
            out->priority = v;
            continue;
        }
        if (strcmp(arg, "-d") == 0 || strcmp(arg, "--due") == 0) {
            if (i + 1 >= argc || !todo_is_valid_date(argv[++i])) {
                fprintf(stderr, "无效的截止日期，格式应为 YYYY-MM-DD\n");
                free(parts);
                return -1;
            }
            out->due = todo_strdup(argv[i]);
            if (out->due == NULL) {
                free(parts);
                return -1;
            }
            continue;
        }
        parts[title_count++] = arg;
    }
    out->title = join_parts(parts, title_count, " ");
    free(parts);
    if (out->title == NULL || *todo_str_trim(out->title) == '\0') {
        fprintf(stderr, "缺少任务标题\n");
        return -1;
    }
    return 0;
}

static int parse_ids(int argc, char **argv, TodoCliArgs *out)
{
    int i;
    long v;

    if (argc == 0) {
        fprintf(stderr, "缺少任务 ID\n");
        return -1;
    }
    out->ids = (long *)calloc((size_t)argc, sizeof(long));
    if (out->ids == NULL)
        return -1;
    for (i = 0; i < argc; i++) {
        if (todo_str_to_long(argv[i], &v) != 0 || v <= 0) {
            fprintf(stderr, "无效的任务 ID: %s\n", argv[i]);
            return -1;
        }
        out->ids[out->id_count++] = v;
    }
    return 0;
}

static int parse_list(int argc, char **argv, TodoCliArgs *out)
{
    int i;

    out->list_filter = TODO_LIST_PENDING;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0)
            out->list_filter = TODO_LIST_ALL;
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--done") == 0)
            out->list_filter = TODO_LIST_DONE;
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--todo") == 0)
            out->list_filter = TODO_LIST_PENDING;
        else {
            fprintf(stderr, "未知选项: %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

static int parse_edit(int argc, char **argv, TodoCliArgs *out)
{
    long id;

    if (argc < 2) {
        fprintf(stderr, "用法: todo edit <ID> <新标题...>\n");
        return -1;
    }
    if (todo_str_to_long(argv[0], &id) != 0 || id <= 0) {
        fprintf(stderr, "无效的任务 ID: %s\n", argv[0]);
        return -1;
    }
    out->ids = (long *)calloc(1, sizeof(long));
    if (out->ids == NULL)
        return -1;
    out->ids[0] = id;
    out->id_count = 1;
    out->title = join_parts((const char *const *)(argv + 1), argc - 1, " ");
    if (out->title == NULL || *todo_str_trim(out->title) == '\0') {
        fprintf(stderr, "缺少新标题\n");
        return -1;
    }
    return 0;
}

int todo_cli_parse(int argc, char **argv, TodoCliArgs *out)
{
    memset(out, 0, sizeof(*out));
    out->cmd = CMD_NONE;

    if (argc < 2) {
        todo_cli_usage();
        return -1;
    }
    out->cmd = match_command(argv[1]);
    if (out->cmd == CMD_NONE) {
        fprintf(stderr, "未知命令: %s\n\n", argv[1]);
        todo_cli_usage();
        return -1;
    }

    switch (out->cmd) {
    case CMD_ADD:
        return parse_add(argc - 2, argv + 2, out);
    case CMD_LIST:
        return parse_list(argc - 2, argv + 2, out);
    case CMD_DONE:
    case CMD_UNDO:
    case CMD_DEL:
        return parse_ids(argc - 2, argv + 2, out);
    case CMD_EDIT:
        return parse_edit(argc - 2, argv + 2, out);
    case CMD_WEATHER:
        if (argc > 2) {
            out->city = join_parts((const char *const *)(argv + 2), argc - 2, " ");
            if (out->city == NULL)
                return -1;
        }
        return 0;
    case CMD_NEWS:
        if (argc > 3) {
            fprintf(stderr, "用法: todo news [订阅源1,订阅源2,...]\n");
            return -1;
        }
        if (argc == 3)
            out->feeds = todo_strdup(argv[2]);
        return 0;
    case CMD_CLEAR:
    case CMD_STATS:
        if (argc > 2) {
            fprintf(stderr, "该命令不接受参数\n");
            return -1;
        }
        return 0;
    case CMD_HELP:
    case CMD_VERSION:
    default:
        return 0;
    }
}

void todo_cli_usage(void)
{
    fprintf(stderr, "用法: todo <命令> [选项] [参数]\n");
    fprintf(stderr, "输入 todo help 查看完整帮助\n");
}

void todo_cli_help(void)
{
    printf(
        "todo - 终端待办事项管理工具\n"
        "\n"
        "用法:\n"
        "  todo <命令> [选项] [参数]\n"
        "\n"
        "命令:\n"
        "  add <标题...> [-p 1|2|3] [-d YYYY-MM-DD]\n"
        "        添加任务，优先级 1 高 / 2 中(默认) / 3 低\n"
        "  list | ls [-a|--all] [-d|--done] [-t|--todo]\n"
        "        列出任务，默认只显示待办\n"
        "  done <ID...>          标记任务为已完成\n"
        "  undo <ID...>          取消任务完成状态\n"
        "  del | rm <ID...>      删除任务\n"
        "  edit <ID> <新标题...>  修改任务标题\n"
        "  clear                 清除全部已完成任务\n"
        "  stats                 任务统计\n"
        "  weather [城市]        查询天气，缺省城市按 IP 自动定位\n"
        "  news [订阅源...]       阅读新闻，逗号分隔 RSS/Atom 订阅源\n"
        "  version               显示版本信息\n"
        "  help                  显示本帮助\n"
        "\n"
        "示例:\n"
        "  todo add 写周报 -p 1 -d 2026-10-01\n"
        "  todo list --all\n"
        "  todo done 1 3\n"
        "  todo weather 北京\n"
        "  todo news\n");
}
