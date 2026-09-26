#ifndef TODO_CONFIG_H
#define TODO_CONFIG_H

typedef struct {
    char data_dir[1024];
    char db_path[1024];
    char script_dir[1024];
    char default_city[256];
    char python[256];
    char news_feeds[4096];
} TodoConfig;

int todo_config_load(TodoConfig *cfg);
const char *todo_config_file_path(void);

#endif
