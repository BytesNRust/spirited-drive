#include "proc_run.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list args;
        va_start(args, format);
        vsnprintf(why, cap, format, args);
        va_end(args);
    }
    return 0;
}

static void log_add(ProcRun *p, const char *data, size_t n)
{
    if (n > PROC_LOG_MAX) {
        data += n - PROC_LOG_MAX;
        n = PROC_LOG_MAX;
        p->log_cut = 1;
    }
    if (p->log_len + n > PROC_LOG_MAX) {   /* the start goes, up to the end of a line */
        size_t drop = p->log_len + n - PROC_LOG_MAX;
        const char *nl = memchr(p->log + drop, '\n', p->log_len - drop);
        if (nl != NULL && (size_t)(nl - (p->log + drop)) < 256) drop = (size_t)(nl - p->log) + 1;
        if (drop > p->log_len) drop = p->log_len;
        memmove(p->log, p->log + drop, p->log_len - drop);
        p->log_len -= drop;
        p->log_cut = 1;
    }
    memcpy(p->log + p->log_len, data, n);
    p->log_len += n;
    p->log[p->log_len] = '\0';
}

static void drain(ProcRun *p)
{
    char buf[4096];
    while (p->fd >= 0) {
        const ssize_t n = read(p->fd, buf, sizeof buf);
        if (n > 0) {
            log_add(p, buf, (size_t)n);
        } else {
            if (n < 0 && errno == EINTR) continue;
            if (n == 0 || errno != EAGAIN) {
                close(p->fd);
                p->fd = -1;
            }
            break;
        }
    }
}

int proc_start(ProcRun *p, char *const argv[], char *const envp[], const char *cwd, char *why, size_t why_cap)
{
    char *log = malloc(PROC_LOG_MAX + 1);
    int pipes[2];
    if (log == NULL || pipe(pipes) != 0) {
        free(log);
        return refuse(why, why_cap, "could not start it: %s", strerror(errno != 0 ? errno : ENOMEM));
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipes[0]);
        close(pipes[1]);
        free(log);
        return refuse(why, why_cap, "could not start it: %s", strerror(errno));
    }
    if (pid == 0) {   /* its own process group (Stop ends all of it), no input, both streams to the pipe */
        setpgid(0, 0);
        const int null = open("/dev/null", O_RDONLY);
        if (null >= 0) dup2(null, 0);
        dup2(pipes[1], 1);
        dup2(pipes[1], 2);
        close(pipes[0]);
        close(pipes[1]);
        if (cwd != NULL && chdir(cwd) != 0) _exit(126);
        execve(argv[0], argv, envp);
        _exit(127);
    }
    setpgid(pid, pid);
    close(pipes[1]);
    fcntl(pipes[0], F_SETFL, fcntl(pipes[0], F_GETFL) | O_NONBLOCK);
    free(p->log);
    memset(p, 0, sizeof *p);
    p->running = 1;
    p->pid = pid;
    p->fd = pipes[0];
    p->log = log;
    p->log[0] = '\0';
    return 1;
}

int proc_poll(ProcRun *p)
{
    if (!p->running) return 0;
    drain(p);
    int status = 0;
    const pid_t done = waitpid(p->pid, &status, WNOHANG);
    if (done != p->pid) return 1;
    drain(p);
    if (p->fd >= 0) close(p->fd);
    p->fd = -1;
    p->running = 0;
    p->finished = 1;
    p->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
    return 0;
}

void proc_stop(ProcRun *p)
{
    if (!p->running) return;
    p->stopped = 1;
    kill(-p->pid, SIGTERM);
}

void proc_free(ProcRun *p)
{
    if (p->running) {
        kill(-p->pid, SIGKILL);
        int status;
        waitpid(p->pid, &status, 0);
    }
    if (p->log != NULL && p->fd >= 0) close(p->fd);   /* a struct that never ran is all zero: its fd 0 is not ours to close */
    free(p->log);
    memset(p, 0, sizeof *p);
    p->fd = -1;
}
