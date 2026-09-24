#include <todo/ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <todo/util.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32

static wchar_t *to_wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w;
    if (n <= 0)
        return NULL;
    w = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)n);
    if (w != NULL)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static int run_windows(const char *python, const char *script,
                       const char *const *args, int arg_count,
                       char *out, size_t cap, int *exit_code)
{
    HANDLE rd = NULL, wr = NULL;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t *cmdline = NULL;
    wchar_t *tokens[64];
    int token_count = 0;
    size_t need = 1, i;
    DWORD n;
    int rc = -1;

    memset(&pi, 0, sizeof(pi));

    tokens[token_count++] = to_wide(python);
    tokens[token_count++] = to_wide(script);
    for (i = 0; i < (size_t)arg_count && token_count < 63; i++)
        tokens[token_count++] = to_wide(args[i]);
    for (i = 0; i < (size_t)token_count; i++) {
        if (tokens[i] == NULL)
            goto done;
        need += wcslen(tokens[i]) + 3;
    }

    cmdline = (wchar_t *)malloc(sizeof(wchar_t) * need);
    if (cmdline == NULL)
        goto done;
    {
        wchar_t *p = cmdline;
        for (i = 0; i < (size_t)token_count; i++) {
            if (i > 0)
                *p++ = L' ';
            *p++ = L'"';
            wcscpy(p, tokens[i]);
            p += wcslen(tokens[i]);
            *p++ = L'"';
        }
        *p = L'\0';
    }

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&rd, &wr, &sa, 0))
        goto done;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = wr;
    si.hStdError = wr;

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, 0,
                        NULL, NULL, &si, &pi))
        goto done;
    CloseHandle(wr);
    wr = NULL;

    {
        size_t off = 0;
        for (;;) {
            char chunk[4096];
            if (!ReadFile(rd, chunk, sizeof(chunk), &n, NULL) || n == 0)
                break;
            if (off + n < cap) {
                memcpy(out + off, chunk, n);
                off += n;
                out[off] = '\0';
            }
        }
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &n))
        n = 1;
    *exit_code = (int)n;
    rc = 0;

done:
    if (wr != NULL)
        CloseHandle(wr);
    if (rd != NULL)
        CloseHandle(rd);
    if (pi.hProcess != NULL)
        CloseHandle(pi.hProcess);
    if (pi.hThread != NULL)
        CloseHandle(pi.hThread);
    for (i = 0; i < (size_t)token_count; i++)
        free(tokens[i]);
    free(cmdline);
    return rc;
}

#else

static int run_unix(const char *python, const char *script,
                    const char *const *args, int arg_count,
                    char *out, size_t cap, int *exit_code)
{
    int fds[2];
    pid_t pid;
    int status;
    char **argv = NULL;
    size_t off = 0;
    int i;

    if (pipe(fds) != 0)
        return -1;

    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);

        argv = (char **)malloc((size_t)(arg_count + 3) * sizeof(char *));
        if (argv == NULL)
            _exit(127);
        argv[0] = (char *)python;
        argv[1] = (char *)script;
        for (i = 0; i < arg_count; i++)
            argv[2 + i] = (char *)args[i];
        argv[2 + arg_count] = NULL;

        execvp(python, argv);
        _exit(127);
    }

    close(fds[1]);
    for (;;) {
        char chunk[4096];
        ssize_t n = read(fds[0], chunk, sizeof(chunk));
        if (n <= 0)
            break;
        if (off + (size_t)n < cap) {
            memcpy(out + off, chunk, (size_t)n);
            off += (size_t)n;
            out[off] = '\0';
        }
    }
    close(fds[0]);

    if (waitpid(pid, &status, 0) < 0)
        *exit_code = -1;
    else if (WIFEXITED(status))
        *exit_code = WEXITSTATUS(status);
    else
        *exit_code = -1;
    return 0;
}

#endif

int todo_ext_run_python(const TodoConfig *cfg, const char *script,
                        const char *const *args, int arg_count,
                        char *out, size_t cap, int *exit_code)
{
    char script_path[1024];

    if (out == NULL || cap == 0)
        return -1;
    out[0] = '\0';
    if (todo_path_join(cfg->script_dir, script, script_path, sizeof(script_path)) != 0) {
        fprintf(stderr, "脚本路径过长\n");
        return -1;
    }
#ifdef _WIN32
    return run_windows(cfg->python, script_path, args, arg_count,
                       out, cap, exit_code);
#else
    return run_unix(cfg->python, script_path, args, arg_count,
                    out, cap, exit_code);
#endif
}
