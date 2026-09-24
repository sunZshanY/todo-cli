#include <todo/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <todo/util.h>

#ifndef TODO_DEFAULT_SCRIPT_DIR
#define TODO_DEFAULT_SCRIPT_DIR "python"
#endif

#ifndef TODO_DEFAULT_PYTHON
#ifdef _WIN32
#define TODO_DEFAULT_PYTHON "python"
#else
#define TODO_DEFAULT_PYTHON "python3"
#endif
#endif

#define DEFAULT_NEWS_FEEDS \
    "https://news.ycombinator.com/rss,http://feeds.bbci.co.uk/news/world/rss.xml"

static void home_dir(char *buf, size_t cap)
{
    const char *h;
#ifdef _WIN32
    h = getenv("USERPROFILE");
    if (h == NULL || *h == '\0')
        h = getenv("HOMEDRIVE");
    /* fall through: use "." */
    if (h == NULL || *h == '\0') {
        snprintf(buf, cap, ".");
        return;
    }
    snprintf(buf, cap, "%s%s", h, getenv("HOMEPATH") != NULL ? getenv("HOMEPATH") : "");
#else
    h = getenv("HOME");
    if (h == NULL || *h == '\0') {
        snprintf(buf, cap, ".");
        return;
    }
    snprintf(buf, cap, "%s", h);
#endif
}

static void config_dir(char *buf, size_t cap)
{
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata != NULL && *appdata != '\0') {
        snprintf(buf, cap, "%s", appdata);
        return;
    }
    home_dir(buf, cap);
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != NULL && *xdg != '\0') {
        snprintf(buf, cap, "%s", xdg);
        return;
    }
    home_dir(buf, cap);
    if (strlen(buf) + 8 < cap)
        strcat(buf, "/.config");
#endif
}

static void default_data_dir(char *buf, size_t cap)
{
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata != NULL && *appdata != '\0') {
        snprintf(buf, cap, "%s/todo-cli", appdata);
        return;
    }
    home_dir(buf, cap);
    if (strlen(buf) + 10 < cap)
        strcat(buf, "/todo-cli");
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg != NULL && *xdg != '\0') {
        snprintf(buf, cap, "%s/todo-cli", xdg);
        return;
    }
    home_dir(buf, cap);
    if (strlen(buf) + 24 < cap)
        strcat(buf, "/.local/share/todo-cli");
#endif
}

const char *todo_config_file_path(void)
{
    static char path[1024];
    static int done = 0;
    if (!done) {
        char dir[900];
        config_dir(dir, sizeof(dir));
        snprintf(path, sizeof(path), "%s/todo-cli/config.ini", dir);
        done = 1;
    }
    return path;
}

static void copy_bounded(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void resolve_script_dir(TodoConfig *cfg)
{
    char exe_dir[1024];
    char candidate[1200];

    if (todo_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        snprintf(candidate, sizeof(candidate), "%s/../share/todo-cli/python", exe_dir);
        if (todo_file_exists(candidate)) {
            copy_bounded(cfg->script_dir, sizeof(cfg->script_dir), candidate);
            return;
        }
        snprintf(candidate, sizeof(candidate), "%s/python", exe_dir);
        if (todo_file_exists(candidate)) {
            copy_bounded(cfg->script_dir, sizeof(cfg->script_dir), candidate);
            return;
        }
    }
    snprintf(cfg->script_dir, sizeof(cfg->script_dir), "%s", TODO_DEFAULT_SCRIPT_DIR);
}

static void apply_key(TodoConfig *cfg, const char *key, const char *value)
{
    if (strcmp(key, "data_dir") == 0) {
        snprintf(cfg->data_dir, sizeof(cfg->data_dir), "%s", value);
    } else if (strcmp(key, "python") == 0) {
        snprintf(cfg->python, sizeof(cfg->python), "%s", value);
    } else if (strcmp(key, "script_dir") == 0) {
        snprintf(cfg->script_dir, sizeof(cfg->script_dir), "%s", value);
    } else if (strcmp(key, "default_city") == 0) {
        snprintf(cfg->default_city, sizeof(cfg->default_city), "%s", value);
    } else if (strcmp(key, "news_feeds") == 0) {
        snprintf(cfg->news_feeds, sizeof(cfg->news_feeds), "%s", value);
    }
}

static void read_config_file(TodoConfig *cfg)
{
    FILE *fp;
    char line[4096];

    fp = fopen(todo_config_file_path(), "r");
    if (fp == NULL)
        return;
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq;
        char *key;
        char *value;
        char *comment;
        comment = strchr(line, '#');
        if (comment == NULL)
            comment = strchr(line, ';');
        if (comment != NULL)
            *comment = '\0';
        todo_str_trim(line);
        if (line[0] == '\0')
            continue;
        eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = '\0';
        key = todo_str_trim(line);
        value = todo_str_trim(eq + 1);
        if (*key != '\0')
            apply_key(cfg, key, value);
    }
    fclose(fp);
}

int todo_config_load(TodoConfig *cfg)
{
    const char *env;

    memset(cfg, 0, sizeof(*cfg));

    default_data_dir(cfg->data_dir, sizeof(cfg->data_dir));
    snprintf(cfg->python, sizeof(cfg->python), "%s", TODO_DEFAULT_PYTHON);
    snprintf(cfg->news_feeds, sizeof(cfg->news_feeds), "%s", DEFAULT_NEWS_FEEDS);
    cfg->default_city[0] = '\0';
    resolve_script_dir(cfg);

    read_config_file(cfg);

    env = getenv("TODO_CLI_DATA_DIR");
    if (env != NULL && *env != '\0')
        snprintf(cfg->data_dir, sizeof(cfg->data_dir), "%s", env);
    env = getenv("TODO_CLI_PYTHON");
    if (env != NULL && *env != '\0')
        snprintf(cfg->python, sizeof(cfg->python), "%s", env);
    env = getenv("TODO_CLI_SCRIPT_DIR");
    if (env != NULL && *env != '\0')
        snprintf(cfg->script_dir, sizeof(cfg->script_dir), "%s", env);

    if (todo_path_join(cfg->data_dir, "todo.db", cfg->db_path, sizeof(cfg->db_path)) != 0)
        return -1;
    return 0;
}
