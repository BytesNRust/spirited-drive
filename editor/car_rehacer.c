#include "car_rehacer.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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

static int copy_file(const char *from, const char *to, mode_t mode, char *why, size_t why_cap)
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
    for (size_t n; ok && (n = fread(buf, 1, sizeof buf, in)) > 0;) ok = fwrite(buf, 1, n, out) == n;
    ok = ok && !ferror(in);
    fclose(in);
    ok = fclose(out) == 0 && ok;
    if (!ok) return refuse(why, why_cap, "could not copy %s: %s", from, strerror(errno != 0 ? errno : EIO));
    chmod(to, mode & 0777);
    return 1;
}

static int copy_tree(const char *from, const char *to, int top, char *why, size_t why_cap)
{
    DIR *d = opendir(from);
    if (d == NULL) return refuse(why, why_cap, "could not read %s: %s", from, strerror(errno));
    int ok = 1;
    for (struct dirent *e; ok && (e = readdir(d)) != NULL;) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || skipped(e->d_name, top)) continue;
        char src[REHACER_PATH], dst[REHACER_PATH];
        struct stat st;
        if (!join(src, sizeof src, from, e->d_name) || !join(dst, sizeof dst, to, e->d_name)) {
            ok = refuse(why, why_cap, "a path in the kit is too long: %s", e->d_name);
        } else if (lstat(src, &st) != 0) {
            ok = refuse(why, why_cap, "could not read %s: %s", src, strerror(errno));
        } else if (S_ISDIR(st.st_mode)) {
            ok = (mkdir(dst, 0777) == 0 || refuse(why, why_cap, "could not make %s: %s", dst, strerror(errno))) && copy_tree(src, dst, 0, why, why_cap);
        } else if (S_ISREG(st.st_mode)) {
            ok = copy_file(src, dst, st.st_mode, why, why_cap);
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
    static char names[256][REHACER_STAMP_NAME];
    const int n = rehacer_backups(kit, names, 256);
    for (int i = 0; i < n - keep; i++) {
        char path[REHACER_PATH], dir[REHACER_PATH];
        if (join(dir, sizeof dir, kit, REHACER_BACKUPS) && join(path, sizeof path, dir, names[i])) remove_tree(path);
    }
}

int rehacer_backup(const char *kit, const char *stamp, int keep, int max_mb, char *out, size_t out_cap, char *why, size_t why_cap)
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
    if (!copy_tree(kit, dest, 1, why, why_cap)) {
        remove_tree(dest);
        return 0;
    }
    snprintf(out, out_cap, "%s", dest);
    rotate(kit, keep);
    return 1;
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

int rehacer_poll(Rehacer *r)
{
    return proc_poll(&r->run);
}

void rehacer_stop(Rehacer *r)
{
    proc_stop(&r->run);
}

void rehacer_free(Rehacer *r)
{
    proc_free(&r->run);   /* a Rehacer that never ran is all zero: proc_free leaves the fd 0 it holds alone */
    memset(r, 0, sizeof *r);
}
