#ifndef TODO_CLI_H
#define TODO_CLI_H

typedef enum {
    CMD_NONE,
    CMD_ADD,
    CMD_LIST,
    CMD_DONE,
    CMD_UNDO,
    CMD_DEL,
    CMD_EDIT,
    CMD_CLEAR,
    CMD_STATS,
    CMD_WEATHER,
    CMD_NEWS,
    CMD_HELP,
    CMD_VERSION
} TodoCommand;

enum {
    TODO_LIST_PENDING = 0,
    TODO_LIST_DONE = 1,
    TODO_LIST_ALL = 2
};

typedef struct {
    TodoCommand cmd;
    char *title;
    long priority;
    char *due;
    int list_filter;
    char *city;
    char *feeds;
    long *ids;
    int id_count;
} TodoCliArgs;

int todo_cli_parse(int argc, char **argv, TodoCliArgs *out);
void todo_cli_usage(void);
void todo_cli_help(void);

#endif
