#include <todo/util.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

char *todo_strdup(const char *s)
{
    size_t n;
    char *p;
    if (s == NULL)
        return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p != NULL)
        memcpy(p, s, n);
    return p;
}

char *todo_str_trim(char *s)
{
    char *end;
    if (s == NULL)
        return NULL;
    while (isspace((unsigned char)*s))
        s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return s;
}

int todo_str_to_long(const char *s, long *out)
{
    char *end = NULL;
    long v;
    if (s == NULL || *s == '\0')
        return -1;
    v = strtol(s, &end, 10);
    if (end == NULL || *end != '\0')
        return -1;
    *out = v;
    return 0;
}

int todo_str_starts_with(const char *s, const char *prefix)
{
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

#ifdef _WIN32
static int mkdir_one(const char *path)
{
    if (CreateDirectoryA(path, NULL))
        return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;
    return -1;
}
#else
static int mkdir_one(const char *path)
{
    if (mkdir(path, 0755) == 0)
        return 0;
    if (errno == EEXIST)
        return 0;
    return -1;
}
#endif

int todo_mkdirs(const char *path)
{
    char buf[1024];
    size_t len = strlen(path);
    size_t i;

    if (len == 0)
        return -1;
    if (len + 1 > sizeof(buf))
        return -1;
    memcpy(buf, path, len + 1);

    for (i = 1; i <= len; i++) {
        if (buf[i] == '/' || buf[i] == '\\' || buf[i] == '\0') {
            char saved = buf[i];
            buf[i] = '\0';
            if (buf[0] != '\0' && mkdir_one(buf) != 0) {
#ifdef _WIN32
                if (i > 3)
#else
                if (strcmp(buf, "/") != 0)
#endif
                    return -1;
            }
            buf[i] = saved;
        }
    }
    return 0;
}

int todo_path_join(const char *dir, const char *name, char *buf, size_t cap)
{
    int n = snprintf(buf, cap, "%s/%s", dir, name);
    return (n >= 0 && (size_t)n < cap) ? 0 : -1;
}

long long todo_time_now(void)
{
    return (long long)time(NULL);
}

int todo_is_valid_date(const char *s)
{
    long y, m, d;
    if (s == NULL || strlen(s) != 10)
        return 0;
    if (s[4] != '-' || s[7] != '-')
        return 0;
    if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
        !isdigit((unsigned char)s[2]) || !isdigit((unsigned char)s[3]) ||
        !isdigit((unsigned char)s[5]) || !isdigit((unsigned char)s[6]) ||
        !isdigit((unsigned char)s[8]) || !isdigit((unsigned char)s[9]))
        return 0;
    y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    m = (s[5] - '0') * 10 + (s[6] - '0');
    d = (s[8] - '0') * 10 + (s[9] - '0');
    (void)y;
    if (m < 1 || m > 12 || d < 1 || d > 31)
        return 0;
    return 1;
}

int todo_is_valid_priority(long p)
{
    return p >= 1 && p <= 3;
}

int todo_exe_dir(char *buf, size_t cap)
{
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)cap);
    char *slash;
    if (n == 0 || n >= cap)
        return -1;
    slash = strrchr(buf, '\\');
    if (slash == NULL)
        slash = strrchr(buf, '/');
    if (slash != NULL)
        *slash = '\0';
    else
        return -1;
    return 0;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)cap;
    char *slash;
    if (_NSGetExecutablePath(buf, &size) != 0)
        return -1;
    slash = strrchr(buf, '/');
    if (slash != NULL)
        *slash = '\0';
    else
        return -1;
    return 0;
#else
    ssize_t n;
    char *slash;
    n = readlink("/proc/self/exe", buf, cap - 1);
    if (n < 0)
        return -1;
    buf[n] = '\0';
    slash = strrchr(buf, '/');
    if (slash != NULL)
        *slash = '\0';
    else
        return -1;
    return 0;
#endif
}

int todo_file_exists(const char *path)
{
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}
