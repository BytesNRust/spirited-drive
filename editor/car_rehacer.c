#include "car_rehacer.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

const char REHACER_SCRIPT[] = "rehacer.command";
const char REHACER_BACKUPS[] = ".studio-backup";

struct RehacerJob {
    pthread_t thread;
    pthread_mutex_t lock;
    int done, cancel;          /* under `lock`: the copy has ended; Stop was asked for */
    char kit[REHACER_PATH], stamp[REHACER_STAMP_NAME];
    int keep, max_mb;
    int ok;                    /* as rehacer_backup returned, with `out` and `why` */
    char out[REHACER_PATH], why[300];
};

static int job_flag(RehacerJob *job, const int *flag)
{
    pthread_mutex_lock(&job->lock);
    const int value = *flag;
    pthread_mutex_unlock(&job->lock);
    return value;
}

/* 1 when the copy of `job` was asked to stop (a copy with no job never is) */
static int cancelled(RehacerJob *job)
{
    return job != NULL && job_flag(job, &job->cancel);
}

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

static int join(char *out, size_t cap, const char *dir, const char *name)
{
    const int n = snprintf(out, cap, "%s/%s", dir, name);
    return n > 0 && (size_t)n < cap;
}

static int is_dir_path(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int rehacer_script_present(const char *kit)
{
    char path[REHACER_PATH];
    struct stat st;
    return kit != NULL && join(path, sizeof path, kit, REHACER_SCRIPT) && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void rehacer_stamp(char *out, size_t cap)
{
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (strftime(out, cap, "%Y%m%d-%H%M%S", &tm) == 0 && cap > 0) out[0] = '\0';
}

/* "YYYYMMDD-HHMMSS" with an optional "-N" after it: the only names of backups, and so the only folders ever removed */
static int stamp_ok(const char *s)
{
    const size_t n = strlen(s);
    if (n < 15 || n >= REHACER_STAMP_NAME) return 0;
    for (size_t i = 0; i < 15; i++)
        if (i == 8 ? s[i] != '-' : s[i] < '0' || s[i] > '9') return 0;
    if (n == 15) return 1;
    if (s[15] != '-' || n == 16) return 0;
    for (size_t i = 16; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* ---- the copy ---- */

/* a folder's entries are skipped when they are not the kit's own work: the backups, Finder's noise */
static int skipped(const char *name, int top)
{
    return strcmp(name, ".DS_Store") == 0 || (top && strcmp(name, REHACER_BACKUPS) == 0);
}

static size_t tree_bytes(const char *dir, int top)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    size_t total = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || skipped(e->d_name, top)) continue;
        char path[REHACER_PATH];
        struct stat st;
        if (!join(path, sizeof path, dir, e->d_name) || lstat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) total += tree_bytes(path, 0);
        else if (S_ISREG(st.st_mode)) total += (size_t)st.st_size;
    }
    closedir(d);
    return total;
}

static int copy_file(const char *from, const char *to, mode_t mode, RehacerJob *job, char *why, size_t why_cap)
{
    FILE *in = fopen(from, "rb");
    if (in == NULL) return refuse(why, why_cap, "could not read %s: %s", from, strerror(errno));
    FILE *out = fopen(to, "wb");
    if (out == NULL) {
        const int err = errno;
        fclose(in);
        return refuse(why, why_cap, "could not write %s: %s", to, strerror(err));
    }
    char buf[65536];
    int ok = 1;
    int stop = 0;
    for (size_t n; ok && !(stop = cancelled(job)) && (n = fread(buf, 1, sizeof buf, in)) > 0;) ok = fwrite(buf, 1, n, out) == n;   /* Stop is seen inside a big file */
    ok = ok && !ferror(in);
    fclose(in);
    ok = fclose(out) == 0 && ok;
    if (stop) return refuse(why, why_cap, "Stop was pressed while the kit was backed up");
    if (!ok) return refuse(why, why_cap, "could not copy %s: %s", from, strerror(errno != 0 ? errno : EIO));
    chmod(to, mode & 0777);
    return 1;
}

static int copy_tree(const char *from, const char *to, int top, RehacerJob *job, char *why, size_t why_cap)
{
    DIR *d = opendir(from);
    if (d == NULL) return refuse(why, why_cap, "could not read %s: %s", from, strerror(errno));
    int ok = 1;
    for (struct dirent *e; ok && (e = readdir(d)) != NULL;) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || skipped(e->d_name, top)) continue;
        if (cancelled(job)) {
            ok = refuse(why, why_cap, "Stop was pressed while the kit was backed up");
            break;
        }
        char src[REHACER_PATH], dst[REHACER_PATH];
        struct stat st;
        if (!join(src, sizeof src, from, e->d_name) || !join(dst, sizeof dst, to, e->d_name)) {
            ok = refuse(why, why_cap, "a path in the kit is too long: %s", e->d_name);
        } else if (lstat(src, &st) != 0) {
            ok = refuse(why, why_cap, "could not read %s: %s", src, strerror(errno));
        } else if (S_ISDIR(st.st_mode)) {
            ok = (mkdir(dst, 0777) == 0 || refuse(why, why_cap, "could not make %s: %s", dst, strerror(errno))) && copy_tree(src, dst, 0, job, why, why_cap);
        } else if (S_ISREG(st.st_mode)) {
            ok = copy_file(src, dst, st.st_mode, job, why, why_cap);
        }   /* a link or a device is not copied */
    }
    closedir(d);
    return ok;
}

/* removes a folder and what is in it (only ever called on a backup this file made or a stamp's own folder) */
static void remove_tree(const char *dir)
{
    DIR *d = opendir(dir);
    if (d != NULL) {
        for (struct dirent *e; (e = readdir(d)) != NULL;) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char path[REHACER_PATH];
            struct stat st;
            if (!join(path, sizeof path, dir, e->d_name) || lstat(path, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) remove_tree(path);
            else unlink(path);
        }
        closedir(d);
    }
    rmdir(dir);
}

static int by_name(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

int rehacer_backups(const char *kit, char names[][REHACER_STAMP_NAME], int cap)
{
    char dir[REHACER_PATH];
    if (kit == NULL || !join(dir, sizeof dir, kit, REHACER_BACKUPS)) return 0;
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int n = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        if (!stamp_ok(e->d_name) || n >= cap) continue;
        snprintf(names[n++], REHACER_STAMP_NAME, "%s", e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, REHACER_STAMP_NAME, by_name);
    return n;
}

static void rotate(const char *kit, int keep)
{
    static char names[256][REHACER_STAMP_NAME];   /* static: only one backup runs at a time (rehacer_begin refuses a second) */
    const int n = rehacer_backups(kit, names, 256);
    for (int i = 0; i < n - keep; i++) {
        char path[REHACER_PATH], dir[REHACER_PATH];
        if (join(dir, sizeof dir, kit, REHACER_BACKUPS) && join(path, sizeof path, dir, names[i])) remove_tree(path);
    }
}

static int backup_with(RehacerJob *job, const char *kit, const char *stamp, int keep, int max_mb, char *out, size_t out_cap, char *why, size_t why_cap)
{
    if (out_cap > 0) out[0] = '\0';
    if (kit == NULL || !is_dir_path(kit)) return refuse(why, why_cap, "the kit folder is not there");
    if (stamp == NULL || !stamp_ok(stamp)) return refuse(why, why_cap, "a backup is named by the time: YYYYMMDD-HHMMSS");
    if (keep < 1) keep = 1;
    if (tree_bytes(kit, 1) > (size_t)max_mb * 1024 * 1024)
        return refuse(why, why_cap, "the kit is larger than %d MB: Studio does not back it up, so it does not run Rehacer", max_mb);
    char root[REHACER_PATH], dest[REHACER_PATH];
    if (!join(root, sizeof root, kit, REHACER_BACKUPS)) return refuse(why, why_cap, "the kit's path is too long");
    if (mkdir(root, 0777) != 0 && errno != EEXIST) return refuse(why, why_cap, "could not make %s: %s", root, strerror(errno));
    int made = 0;
    for (int n = 1; n < 1000 && !made; n++) {
        char name[REHACER_STAMP_NAME];
        if (n == 1) snprintf(name, sizeof name, "%s", stamp);
        else snprintf(name, sizeof name, "%.15s-%d", stamp, n);
        if (!join(dest, sizeof dest, root, name)) return refuse(why, why_cap, "the kit's path is too long");
        made = mkdir(dest, 0777) == 0;
        if (!made && errno != EEXIST) return refuse(why, why_cap, "could not make %s: %s", dest, strerror(errno));
    }
    if (!made) return refuse(why, why_cap, "could not find a free name for the backup");
    if (!copy_tree(kit, dest, 1, job, why, why_cap)) {
        remove_tree(dest);
        return 0;
    }
    snprintf(out, out_cap, "%s", dest);
    rotate(kit, keep);
    return 1;
}

int rehacer_backup(const char *kit, const char *stamp, int keep, int max_mb, char *out, size_t out_cap, char *why, size_t why_cap)
{
    return backup_with(NULL, kit, stamp, keep, max_mb, out, out_cap, why, why_cap);
}

/* ---- the run (proc_run.c) ---- */

int rehacer_start(Rehacer *r, const char *kit, char *why, size_t why_cap)
{
    if (!rehacer_script_present(kit)) return refuse(why, why_cap, "this kit has no %s", REHACER_SCRIPT);
    char *const argv[] = { "/bin/sh", (char *)REHACER_SCRIPT, NULL };   /* the script through sh, the kit as its folder, no input, both streams to the log */
    char backup[REHACER_PATH];
    snprintf(backup, sizeof backup, "%s", r->backup);
    if (!proc_start(&r->run, argv, environ, kit, why, why_cap)) return 0;
    snprintf(r->backup, sizeof r->backup, "%s", backup);   /* the backup made for this run stays with it */
    return 1;
}

/* ---- the backup in its own thread, then the script (owner test R1) ---- */

static void *backup_thread(void *arg)
{
    RehacerJob *job = arg;
    job->ok = backup_with(job, job->kit, job->stamp, job->keep, job->max_mb, job->out, sizeof job->out, job->why, sizeof job->why);
    pthread_mutex_lock(&job->lock);
    job->done = 1;
    pthread_mutex_unlock(&job->lock);
    return NULL;
}

/* waits for the copy (it has ended, or was asked to stop) and frees the job */
static void join_job(Rehacer *r)
{
    pthread_join(r->job->thread, NULL);
    pthread_mutex_destroy(&r->job->lock);
    free(r->job);
    r->job = NULL;
    r->backing_up = 0;
}

int rehacer_busy(const Rehacer *r)
{
    return r->job != NULL || r->run.running;
}

int rehacer_begin(Rehacer *r, const char *kit, const char *stamp, int keep, int max_mb, char *why, size_t why_cap)
{
    if (rehacer_busy(r)) return refuse(why, why_cap, "Rehacer is already running");
    if (!rehacer_script_present(kit)) return refuse(why, why_cap, "this kit has no %s", REHACER_SCRIPT);
    if (stamp == NULL || !stamp_ok(stamp)) return refuse(why, why_cap, "a backup is named by the time: YYYYMMDD-HHMMSS");
    RehacerJob *job = calloc(1, sizeof *job);
    if (job == NULL) return refuse(why, why_cap, "out of memory");
    snprintf(job->kit, sizeof job->kit, "%s", kit);
    snprintf(job->stamp, sizeof job->stamp, "%s", stamp);
    job->keep = keep;
    job->max_mb = max_mb;
    static int last_id;   /* the UI thread is the only one that begins runs */
    rehacer_free(r);      /* the last run and its log */
    r->run_id = ++last_id;
    if (pthread_mutex_init(&job->lock, NULL) != 0) {
        free(job);
        return refuse(why, why_cap, "could not start the backup");
    }
    const int err = pthread_create(&job->thread, NULL, backup_thread, job);
    if (err != 0) {
        pthread_mutex_destroy(&job->lock);
        free(job);
        return refuse(why, why_cap, "could not start the backup: %s", strerror(err));
    }
    r->job = job;
    r->backing_up = 1;
    return 1;
}

/* the copy has ended: the script runs, unless the copy failed or Stop came first. 1 when the script runs. */
static int after_backup(Rehacer *r)
{
    char kit[REHACER_PATH];
    RehacerJob *job = r->job;
    const int ok = job->ok;
    snprintf(kit, sizeof kit, "%s", job->kit);
    if (ok) snprintf(r->backup, sizeof r->backup, "%s", job->out);
    else snprintf(r->why, sizeof r->why, "the backup failed: %s", job->why);
    join_job(r);
    if (r->run.stopped) {   /* Stop while it copied (a copy that ended all the same is kept) */
        r->failed = 1;
        snprintf(r->why, sizeof r->why, "%s", "Stop was pressed before the script ran: it was stopped, nothing ran");
        return 0;
    }
    if (!ok) {
        r->failed = 1;
        r->backup[0] = '\0';
        return 0;
    }
    char why[sizeof r->why];
    if (!rehacer_start(r, kit, why, sizeof why)) {
        r->failed = 1;
        snprintf(r->why, sizeof r->why, "the script did not start: %s", why);
        return 0;
    }
    return 1;
}

int rehacer_poll(Rehacer *r)
{
    if (r->job != NULL) {
        if (!job_flag(r->job, &r->job->done)) return 1;
        return after_backup(r);
    }
    return proc_poll(&r->run);
}

void rehacer_stop(Rehacer *r)
{
    if (r->job != NULL) {
        pthread_mutex_lock(&r->job->lock);
        r->job->cancel = 1;
        pthread_mutex_unlock(&r->job->lock);
        r->run.stopped = 1;
        return;
    }
    proc_stop(&r->run);
}

void rehacer_free(Rehacer *r)
{
    if (r->job != NULL) {   /* the copy stops at its next file, and its partial folder is removed */
        pthread_mutex_lock(&r->job->lock);
        r->job->cancel = 1;
        pthread_mutex_unlock(&r->job->lock);
        join_job(r);
    }
    proc_free(&r->run);   /* a Rehacer that never ran is all zero: proc_free leaves the fd 0 it holds alone */
    memset(r, 0, sizeof *r);
}

int rehacer_version_unraised(const char *log)
{
    /* the game's words: "its files changed but its version did not: raise version= in mod.ini" (the menu starts it with "Its") */
    return log != NULL && strstr(log, "files changed but its version did not") != NULL;
}
