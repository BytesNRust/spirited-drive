/* port: see data_copy.h. */
#include "game/sys/data_copy.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    DataCopyProgress progress;
    void *user;
    int64_t bytes;
    int failed;
} Job;

int64_t data_copy_size(const char *src)
{
    DIR *d = opendir(src);
    if (d == NULL) return -1;
    int64_t total = 0;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        char path[1024];
        struct stat st;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || snprintf(path, sizeof path, "%s/%s", src, e->d_name) >= (int)sizeof path) continue;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            const int64_t sub = data_copy_size(path);
            if (sub > 0) total += sub;
        } else if (S_ISREG(st.st_mode)) {
            total += st.st_size;
        }
    }
    closedir(d);
    return total;
}

static int copy_file(const char *from, const char *to, Job *job)
{
    FILE *in = fopen(from, "rb"), *out = in != NULL ? fopen(to, "wb") : NULL;
    if (in == NULL || out == NULL) {
        if (in != NULL) fclose(in);
        return 0;
    }
    static unsigned char buf[1 << 16];
    int ok = 1;
    for (size_t n = fread(buf, 1, sizeof buf, in); n > 0 && ok; n = fread(buf, 1, sizeof buf, in)) {
        ok = fwrite(buf, 1, n, out) == n;
        job->bytes += (int64_t)n;
    }
    ok = ok && !ferror(in);
    fclose(in);
    return fclose(out) == 0 && ok;
}

static void copy_dir(const char *src, const char *dest, Job *job)
{
    if (job->failed) return;
    if (mkdir(dest, 0755) != 0 && errno != EEXIST) {
        job->failed = 1;
        return;
    }
    DIR *d = opendir(src);
    if (d == NULL) {
        job->failed = 1;
        return;
    }
    for (struct dirent *e = readdir(d); e != NULL && !job->failed; e = readdir(d)) {
        char from[1024], to[1024];
        struct stat st;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || strcmp(e->d_name, ".DS_Store") == 0) continue;
        if (snprintf(from, sizeof from, "%s/%s", src, e->d_name) >= (int)sizeof from || snprintf(to, sizeof to, "%s/%s", dest, e->d_name) >= (int)sizeof to ||
            stat(from, &st) != 0) {
            job->failed = 1;
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            copy_dir(from, to, job);
        } else if (S_ISREG(st.st_mode)) {
            if (!copy_file(from, to, job)) job->failed = 1;
            else if (job->progress != NULL && !job->progress(job->user, job->bytes)) job->failed = 1;   /* cancelled */
        }
    }
    closedir(d);
}

static void remove_tree(const char *path)
{
    DIR *d = opendir(path);
    if (d != NULL) {
        for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
            char sub[1024];
            struct stat st;
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || snprintf(sub, sizeof sub, "%s/%s", path, e->d_name) >= (int)sizeof sub) continue;
            if (lstat(sub, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) remove_tree(sub);
            else unlink(sub);
        }
        closedir(d);
    }
    rmdir(path);
}

int data_copy_tree(const char *src, const char *dest, DataCopyProgress progress, void *user)
{
    char part[1100];
    struct stat st;
    if (stat(dest, &st) == 0 || snprintf(part, sizeof part, "%s.part", dest) >= (int)sizeof part) return 0;
    remove_tree(part);   /* what an interrupted copy left */
    Job job = { progress, user, 0, 0 };
    copy_dir(src, part, &job);
    if (job.failed || rename(part, dest) != 0) {
        remove_tree(part);
        return 0;
    }
    return 1;
}
