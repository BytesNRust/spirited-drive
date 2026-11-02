/* port: see data_copy.h. */
#include "game/sys/data_copy.h"

#include "game/sys/port_log.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MAX_DEPTH = 16 };   /* the CD's tree is three levels deep: a deeper one is a loop or not the CD */

static char error_text[1300];

const char *data_copy_error(void) { return error_text; }

/* what went wrong, with the path and the OS's reason: kept for the caller and in tnfs.log (the first failure only) */
static void fail(const char *what, const char *path, int err)
{
    if (error_text[0] != '\0') return;
    snprintf(error_text, sizeof error_text, "could not %s %s: %s", what, path, err != 0 ? strerror(err) : "stopped");
    port_log("data_copy: %s (errno %d)\n", error_text, err);
}

typedef struct {
    DataCopyProgress progress;
    void *user;
    int64_t bytes;
    int failed;
} Job;

/* What a directory entry is, for the copy: a regular file (a link to one counts as the file), a folder that is really one (never a link to one: a link can loop), or neither. */
typedef enum { KIND_OTHER, KIND_FILE, KIND_DIR } Kind;

static Kind kind_of(const char *path, struct stat *st)
{
    struct stat link;
    if (lstat(path, &link) != 0) return KIND_OTHER;
    if (S_ISDIR(link.st_mode)) return *st = link, KIND_DIR;
    if (S_ISREG(link.st_mode)) return *st = link, KIND_FILE;
    if (S_ISLNK(link.st_mode) && stat(path, st) == 0 && S_ISREG(st->st_mode)) return KIND_FILE;
    return KIND_OTHER;
}

static int64_t size_of(const char *src, int depth)
{
    DIR *d = depth < MAX_DEPTH ? opendir(src) : NULL;
    if (d == NULL) return -1;
    int64_t total = 0;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        char path[1024];
        struct stat st;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || snprintf(path, sizeof path, "%s/%s", src, e->d_name) >= (int)sizeof path) continue;
        const Kind k = kind_of(path, &st);
        if (k == KIND_DIR) {
            const int64_t sub = size_of(path, depth + 1);
            if (sub > 0) total += sub;
        } else if (k == KIND_FILE) {
            total += st.st_size;
        }
    }
    closedir(d);
    return total;
}

int64_t data_copy_size(const char *src) { return size_of(src, 0); }

static int copy_file(const char *from, const char *to, Job *job)
{
    FILE *in = fopen(from, "rb");
    if (in == NULL) return fail("read", from, errno), 0;
    FILE *out = fopen(to, "wb");
    if (out == NULL) return fail("create", to, errno), fclose(in), 0;
    static unsigned char buf[1 << 16];
    int ok = 1, err = 0;
    for (size_t n = fread(buf, 1, sizeof buf, in); n > 0 && ok; n = fread(buf, 1, sizeof buf, in)) {
        ok = fwrite(buf, 1, n, out) == n;
        if (!ok) err = errno;
        job->bytes += (int64_t)n;
    }
    if (ok && ferror(in)) ok = 0, err = errno;
    fclose(in);
    if (fclose(out) != 0 && ok) ok = 0, err = errno;
    if (!ok) fail("write", to, err);
    return ok;
}

static void copy_dir(const char *src, const char *dest, Job *job, int depth)
{
    if (job->failed) return;
    if (depth >= MAX_DEPTH) {
        job->failed = 1;
        fail("go deeper than", src, ELOOP);
        return;
    }
    if (mkdir(dest, 0755) != 0 && errno != EEXIST) {
        job->failed = 1;
        fail("create the folder", dest, errno);
        return;
    }
    DIR *d = opendir(src);
    if (d == NULL) {
        job->failed = 1;
        fail("open the folder", src, errno);
        return;
    }
    for (struct dirent *e = readdir(d); e != NULL && !job->failed; e = readdir(d)) {
        char from[1024], to[1024];
        struct stat st;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || strcmp(e->d_name, ".DS_Store") == 0) continue;
        if (snprintf(from, sizeof from, "%s/%s", src, e->d_name) >= (int)sizeof from || snprintf(to, sizeof to, "%s/%s", dest, e->d_name) >= (int)sizeof to) {
            job->failed = 1;
            fail("use the name", e->d_name, ENAMETOOLONG);
            break;
        }
        const Kind k = kind_of(from, &st);
        if (k == KIND_DIR) {
            copy_dir(from, to, job, depth + 1);
        } else if (k == KIND_FILE) {
            if (!copy_file(from, to, job)) job->failed = 1;
            else if (job->progress != NULL && !job->progress(job->user, job->bytes)) job->failed = 1, fail("finish", from, 0);   /* cancelled */
        }
    }
    closedir(d);
}

/* Removes a copy's leftovers, read-only files and folders included (a CD's files are r--------, and a copy made from them can be too); links are removed, never followed. */
static void remove_tree(const char *path, int depth)
{
    struct stat st;
    if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode) && depth < MAX_DEPTH) {
        chmod(path, 0700);
        DIR *d = opendir(path);
        if (d != NULL) {
            for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
                char sub[1024];
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || snprintf(sub, sizeof sub, "%s/%s", path, e->d_name) >= (int)sizeof sub) continue;
                if (lstat(sub, &st) != 0) continue;
                if (S_ISDIR(st.st_mode)) remove_tree(sub, depth + 1);
                else unlink(sub);
            }
            closedir(d);
        }
        rmdir(path);
    } else {
        unlink(path);
    }
}

int data_copy_tree(const char *src, const char *dest, DataCopyProgress progress, void *user)
{
    char part[1100], old[1100];
    struct stat st;
    error_text[0] = '\0';
    if (snprintf(part, sizeof part, "%s.part", dest) >= (int)sizeof part || snprintf(old, sizeof old, "%s.old", dest) >= (int)sizeof old)
        return fail("use the name", dest, ENAMETOOLONG), 0;
    remove_tree(part, 0);   /* what an interrupted copy left */
    remove_tree(old, 0);
    Job job = { progress, user, 0, 0 };
    copy_dir(src, part, &job, 0);
    if (job.failed) {
        remove_tree(part, 0);
        return 0;
    }
    const int replacing = lstat(dest, &st) == 0;
    /* a copy already there is moved aside, the new one into place, and only then the old one deleted: a failure at any step leaves one whole copy */
    if (replacing && rename(dest, old) != 0) {
        fail("move aside", dest, errno);
        remove_tree(part, 0);
        return 0;
    }
    if (rename(part, dest) != 0) {
        const int why = errno;
        if (replacing) (void)rename(old, dest);
        fail("move into place", dest, why);
        remove_tree(part, 0);
        return 0;
    }
    if (replacing) remove_tree(old, 0);
    return 1;
}
