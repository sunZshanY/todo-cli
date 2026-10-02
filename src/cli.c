#include <todo/cli.h>

#include <ctype.h>
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
    if (strcmp(s, "project") == 0)
        return CMD_PROJECT;
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
    if (strcmp(s, "trash") == 0)
        return CMD_TRASH;
    if (strcmp(s, "restore") == 0)
        return CMD_RESTORE;
    if (strcmp(s, "purge") == 0)
        return CMD_PURGE;
    if (strcmp(s, "cal") == 0 || strcmp(s, "-cal") == 0)
        return CMD_CAL;
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

static int parse_priority(const char *s, long *out)
{
    if (s[0] == 'P' || s[0] == 'p')
        s++;
    if (strlen(s) != 1 || s[0] < '0' || s[0] > '3') {
        fprintf(stderr, "无效的优先级，应为 P0/P1/P2/P3（也支持数字 0/1/2/3）\n");
        return -1;
    }
    *out = s[0] - '0';
    return 0;
}

static int set_project(TodoCliArgs *out, const char *name)
{
    char *copy, *trimmed;
    if (out->project != NULL) {
        fprintf(stderr, "项目只能指定一次\n");
        return -1;
    }
    copy = todo_strdup(name);
    if (copy == NULL)
        return -1;
    trimmed = todo_str_trim(copy);
    if (*trimmed == '\0') {
        fprintf(stderr, "项目名称不能为空\n");
        free(copy);
        return -1;
    }
    out->project = todo_strdup(trimmed);
    free(copy);
    return out->project != NULL ? 0 : -1;
}

static int parse_project(int argc, char **argv, TodoCliArgs *out)
{
    if (argc == 0 || (argc == 1 &&
        (strcmp(argv[0], "list") == 0 || strcmp(argv[0], "ls") == 0))) {
        out->project_cmd = PROJECT_LIST;
        return 0;
    }
    if (argc == 2 && (strcmp(argv[0], "add") == 0 || strcmp(argv[0], "show") == 0)) {
        out->project_cmd = strcmp(argv[0], "add") == 0 ? PROJECT_ADD : PROJECT_SHOW;
        return set_project(out, argv[1]);
    }
    fprintf(stderr, "用法: todo project add <项目> | list | show <项目>\n");
    return -1;
}

static int parse_add(int argc, char **argv, TodoCliArgs *out)
{
    int i;
    int title_count = 0, title_start = 0, literal_title = 0, priority_set = 0;
    const char **parts;
    long v;

    out->priority = 2;
    parts = (const char **)calloc((size_t)(argc > 0 ? argc : 1), sizeof(char *));
    if (parts == NULL)
        return -1;

    for (i = 0; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            literal_title = title_count == 0;
            for (i++; i < argc; i++)
                parts[title_count++] = argv[i];
            break;
        }
        if (strcmp(arg, "-p") == 0 || strcmp(arg, "--priority") == 0) {
            if (priority_set || i + 1 >= argc || parse_priority(argv[++i], &v) != 0) {
                fprintf(stderr, "--priority 需要一个 P0/P1/P2/P3 值，且只能指定一次\n");
                free(parts);
                return -1;
            }
            out->priority = v;
            priority_set = 1;
            continue;
        }
        if (strcmp(arg, "-d") == 0 || strcmp(arg, "--due") == 0) {
            if (out->due != NULL || i + 1 >= argc || !todo_is_valid_date(argv[++i])) {
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
        if (strcmp(arg, "--project") == 0) {
            if (i + 1 >= argc || set_project(out, argv[++i]) != 0) {
                fprintf(stderr, "--project 需要一个项目名称\n");
                free(parts);
                return -1;
            }
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "未知选项: %s（以 - 开头的标题请放在 -- 后）\n", arg);
            free(parts);
            return -1;
        }
        parts[title_count++] = arg;
    }
    if (out->project == NULL && title_count >= 2 && !literal_title) {
        if (set_project(out, parts[0]) != 0) {
            free(parts);
            return -1;
        }
        title_start = 1;
    }
    if (out->project == NULL && set_project(out, "TODO") != 0) {
        free(parts);
        return -1;
    }
    out->title = join_parts(parts + title_start, title_count - title_start, " ");
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
    int i, status_set = 0;

    out->list_filter = TODO_LIST_PENDING;
    for (i = 0; i < argc; i++) {
        int filter = -1;
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0)
            filter = TODO_LIST_ALL;
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--done") == 0)
            filter = TODO_LIST_DONE;
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--todo") == 0 ||
                 strcmp(argv[i], "--pending") == 0)
            filter = TODO_LIST_PENDING;
        else if (strcmp(argv[i], "--project") == 0) {
            if (i + 1 >= argc || set_project(out, argv[++i]) != 0) {
                fprintf(stderr, "--project 需要一个项目名称\n");
                return -1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--priority") == 0) {
            if (out->priority != -1 || i + 1 >= argc ||
                parse_priority(argv[++i], &out->priority) != 0) {
                fprintf(stderr, "--priority 需要一个 P0/P1/P2/P3 值，且只能指定一次\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--") == 0 && i + 2 == argc) {
            return set_project(out, argv[i + 1]);
        } else if (argv[i][0] != '-') {
            if (set_project(out, argv[i]) != 0)
                return -1;
        }
        else {
            fprintf(stderr, "未知选项: %s\n", argv[i]);
            return -1;
        }
        if (filter != -1) {
            if (status_set && filter != out->list_filter) {
                fprintf(stderr, "--all、--done 和 --todo 不能同时使用\n");
                return -1;
            }
            out->list_filter = filter;
            status_set = 1;
        }
    }
    return 0;
}

static int parse_edit(int argc, char **argv, TodoCliArgs *out)
{
    long id, v;
    int i, title_count = 0, priority_set = 0;
    const char **parts;

    if (argc < 1) {
        fprintf(stderr, "用法: todo edit <ID> [新标题...] [-p P0|P1|P2|P3] [-d YYYY-MM-DD]\n");
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

    parts = (const char **)calloc((size_t)(argc > 0 ? argc : 1), sizeof(char *));
    if (parts == NULL)
        return -1;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--priority") == 0) {
            if (priority_set || i + 1 >= argc || parse_priority(argv[++i], &v) != 0) {
                fprintf(stderr, "--priority 需要一个 P0/P1/P2/P3 值，且只能指定一次\n");
                free(parts);
                return -1;
            }
            out->priority = v;
            priority_set = 1;
            continue;
        }
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--due") == 0) {
            if (out->due != NULL || i + 1 >= argc || !todo_is_valid_date(argv[++i])) {
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
        if (argv[i][0] == '-') {
            fprintf(stderr, "未知选项: %s\n", argv[i]);
            free(parts);
            return -1;
        }
        parts[title_count++] = argv[i];
    }
    if (title_count > 0) {
        out->title = join_parts(parts, title_count, " ");
        free(parts);
        if (out->title == NULL || *todo_str_trim(out->title) == '\0') {
            fprintf(stderr, "缺少新标题\n");
            return -1;
        }
    } else {
        free(parts);
    }
    if (out->title == NULL && out->priority < 0 && out->due == NULL) {
        fprintf(stderr, "缺少新标题或修改选项（-p 优先级 / -d 截止日期）\n");
        return -1;
    }
    return 0;
}

static int parse_month(const char *s, int *year, int *month)
{
    if (s == NULL || strlen(s) != 7 || s[4] != '-')
        return -1;
    if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
        !isdigit((unsigned char)s[2]) || !isdigit((unsigned char)s[3]) ||
        !isdigit((unsigned char)s[5]) || !isdigit((unsigned char)s[6]))
        return -1;
    *year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    *month = (s[5] - '0') * 10 + (s[6] - '0');
    if (*year < 1 || *month < 1 || *month > 12)
        return -1;
    return 0;
}

static int parse_cal(int argc, char **argv, TodoCliArgs *out)
{
    if (argc == 0)
        return 0;
    if (argc == 1 && parse_month(argv[0], &out->cal_year, &out->cal_month) == 0) {
        out->cal_set = 1;
        return 0;
    }
    fprintf(stderr, "用法: todo cal [YYYY-MM]\n");
    return -1;
}

static int parse_trash(int argc, char **argv, TodoCliArgs *out)
{
    (void)argv;
    (void)out;
    if (argc != 0) {
        fprintf(stderr, "该命令不接受参数\n");
        return -1;
    }
    return 0;
}

static int parse_purge(int argc, char **argv, TodoCliArgs *out)
{
    if (argc == 1 && strcmp(argv[0], "--all") == 0) {
        out->purge_all = 1;
        return 0;
    }
    if (argc > 0 && argv[0][0] == '-') {
        fprintf(stderr, "用法: todo purge <回收ID...> | --all\n");
        return -1;
    }
    return parse_ids(argc, argv, out);
}

int todo_cli_parse(int argc, char **argv, TodoCliArgs *out)
{
    int i;
    memset(out, 0, sizeof(*out));
    out->cmd = CMD_NONE;
    out->priority = -1;

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

    for (i = 2; i < argc && strcmp(argv[i], "--") != 0; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            out->cmd = CMD_HELP;
            return 0;
        }
    }
    switch (out->cmd) {
    case CMD_PROJECT:
        return parse_project(argc - 2, argv + 2, out);
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
    case CMD_TRASH:
        return parse_trash(argc - 2, argv + 2, out);
    case CMD_RESTORE:
        return parse_ids(argc - 2, argv + 2, out);
    case CMD_PURGE:
        return parse_purge(argc - 2, argv + 2, out);
    case CMD_CAL:
        return parse_cal(argc - 2, argv + 2, out);
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
    fprintf(stderr, "输入 todo --help 查看完整帮助\n");
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
        "  project add <项目>     创建自定义项目，支持中文和空格\n"
        "  project list           列出项目及完成进度（也可直接 todo project）\n"
        "  project show <项目>    按 P0/P1/P2/P3、Completed 展示任务及 Progress\n"
        "  add <项目> <标题...> [-p P0|P1|P2|P3] [-d YYYY-MM-DD]\n"
        "        添加项目任务；P0 最高，P3 最低，默认 P2；支持 --priority\n"
        "  add <标题> [--project <项目>] [--priority P0|P1|P2|P3]\n"
        "        单个标题参数默认归入 TODO 项目；含空格的标题请加引号\n"
        "  list | ls [项目] [--project <项目>] [-p|--priority P0|P1|P2|P3]\n"
        "        [-a|--all] [-d|--done] [-t|--todo|--pending]\n"
        "        默认显示所有项目的待办，可组合项目、优先级和状态筛选\n"
        "  done <ID...>          标记任务为已完成\n"
        "  undo <ID...>          取消任务完成状态\n"
        "  delete <ID...>        删除任务到回收站（别名 del、rm）\n"
        "  trash                 查看回收站中的任务内容\n"
        "  restore <ID...>       从回收站恢复任务（使用 trash 输出中的回收ID）\n"
        "  purge <回收ID...>      永久删除回收站任务；purge --all 清空回收站\n"
        "  edit <ID> [新标题...] [-p P0|P1|P2|P3] [-d YYYY-MM-DD]\n"
        "        修改任务标题、优先级或截止日期（至少提供一项）\n"
        "  cal | -cal [YYYY-MM]  查看月历与提醒事项，默认当前月份\n"
        "  clear                 清除全部已完成任务（移入回收站）\n"
        "  stats                 任务统计\n"
        "  weather [城市]        查询天气，缺省城市按 IP 自动定位\n"
        "  news [订阅源...]       阅读新闻，逗号分隔 RSS/Atom 订阅源\n"
        "  version               显示版本信息\n"
        "  --help | -h | help    显示本帮助\n"
        "\n"
        "示例:\n"
        "  todo project add NanoX\n"
        "  todo project add Python学习\n"
        "  todo add NanoX \"修复崩溃\" --priority P0\n"
        "  todo project show NanoX\n"
        "  todo list NanoX --priority P0 --todo\n"
        "  todo add \"复习一元一次方程\"\n"
        "  todo list\n"
        "  todo done 1\n"
        "  todo delete 1\n"
        "  todo trash\n"
        "  todo restore 1\n"
        "  todo edit 2 \"新标题\" --priority P0\n"
        "  todo cal\n"
        "  todo --help\n"
        "  todo weather 北京\n"
        "  todo news\n"
        "\n"
        "任务 ID 在所有项目中唯一；done/delete 使用输出中的 ID。\n"
        "旧数据库自动升级，任务归入 TODO，ID 与完成状态保持不变。\n"
        "数据库默认位于 ~/.local/share/todo-cli/todo.db（Linux），\n"
        "可用 TODO_CLI_DATA_DIR 覆盖；支持数字优先级 0/1/2/3。\n");
}
