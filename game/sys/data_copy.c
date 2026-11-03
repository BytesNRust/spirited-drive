/* port: see data_copy.h. */
#include "game/sys/data_copy.h"

#include "game/sys/port_log.h"
#include "iso/iso9660.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

/* the copy's two staging names (`dest`.part and `dest`.old) cleared of an interrupted copy's leftovers; 0 when the names do not fit */
static int begin_copy(const char *dest, char part[1100], char old[1100])
{
    error_text[0] = '\0';
    if (snprintf(part, 1100, "%s.part", dest) >= 1100 || snprintf(old, 1100, "%s.old", dest) >= 1100) return fail("use the name", dest, ENAMETOOLONG), 0;
    remove_tree(part, 0);
    remove_tree(old, 0);
    return 1;
}

/* the finished `part` into place: a copy already at `dest` is moved aside, the new one in, and only then the old one deleted: a failure at any step leaves one whole copy */
static int finish_copy(const char *dest, const char *part, const char *old, const Job *job)
{
    struct stat st;
    if (job->failed) {
        remove_tree(part, 0);
        return 0;
    }
    const int replacing = lstat(dest, &st) == 0;
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

int data_copy_tree(const char *src, const char *dest, DataCopyProgress progress, void *user)
{
    char part[1100], old[1100];
    if (!begin_copy(dest, part, old)) return 0;
    Job job = { progress, user, 0, 0 };
    copy_dir(src, part, &job, 0);
    return finish_copy(dest, part, old, &job);
}

/* ---- port (ISO): the same copy out of an image ---- */

enum { MAX_ISO_DIRS = 512 };

typedef struct {
    Job job;
    IsoImage *iso;
    uint32_t seen[MAX_ISO_DIRS];   /* the folders entered, by their sector: a folder met again is a loop */
    int seen_count;
} IsoJob;

static void join(char *out, size_t cap, const char *dir, const char *name) { snprintf(out, cap, "%s%s%s", dir, dir[0] != '\0' ? "/" : "", name); }

static int iso_seen(IsoJob *j, uint32_t lba)
{
    for (int i = 0; i < j->seen_count; i++)
        if (j->seen[i] == lba) return 1;
    if (j->seen_count >= MAX_ISO_DIRS) return 1;   /* more folders than a CD has: stop */
    j->seen[j->seen_count++] = lba;
    return 0;
}

static int64_t iso_size_of(IsoJob *j, const char *dir, int depth)
{
    IsoEntry here;
    if (depth >= MAX_DEPTH || !iso_stat(j->iso, dir, &here) || !here.is_dir) return -1;
    const int n = iso_list(j->iso, dir, NULL, 0);
    IsoEntry *list = n > 0 ? malloc((size_t)n * sizeof *list) : NULL;
    if (n < 0 || (n > 0 && (list == NULL || iso_list(j->iso, dir, list, n) != n))) return free(list), -1;
    int64_t total = 0;
    for (int i = 0; i < n; i++) {
        char path[1100];
        join(path, sizeof path, dir, list[i].name);
        if (!list[i].is_dir) total += list[i].size;
        else if (!iso_seen(j, list[i].lba)) {
            const int64_t sub = iso_size_of(j, path, depth + 1);
            if (sub > 0) total += sub;
        }
    }
    free(list);
    return total;
}

static int copy_iso_file(IsoImage *iso, const IsoEntry *e, const char *to, IsoJob *j)
{
    FILE *out = fopen(to, "wb");
    if (out == NULL) return fail("create", to, errno), 0;
    static unsigned char buf[1 << 16];
    int ok = 1, err = 0;
    int64_t off = 0;
    while (ok && off < (int64_t)e->size) {
        const int64_t n = iso_read(iso, e, off, buf, sizeof buf);
        if (n <= 0) ok = 0, err = EIO;
        else if (fwrite(buf, 1, (size_t)n, out) != (size_t)n) ok = 0, err = errno;
        else off += n, j->job.bytes += n;
    }
    if (fclose(out) != 0 && ok) ok = 0, err = errno;
    if (!ok) fail("write", to, err);
    return ok;
}

static void copy_iso_dir(IsoJob *j, const char *dir, const char *dest, int depth)
{
    Job *job = &j->job;
    if (job->failed) return;
    if (depth >= MAX_DEPTH) {
        job->failed = 1;
        fail("go deeper than", dir, ELOOP);
        return;
    }
    if (mkdir(dest, 0755) != 0 && errno != EEXIST) {
        job->failed = 1;
        fail("create the folder", dest, errno);
        return;
    }
    const int n = iso_list(j->iso, dir, NULL, 0);
    IsoEntry *list = n > 0 ? malloc((size_t)n * sizeof *list) : NULL;
    if (n < 0 || (n > 0 && (list == NULL || iso_list(j->iso, dir, list, n) != n))) {
        job->failed = 1;
        fail("read the folder", dir[0] != '\0' ? dir : "(the image's root)", EIO);
        free(list);
        return;
    }
    for (int i = 0; i < n && !job->failed; i++) {
        char from[1100], to[1100];
        join(from, sizeof from, dir, list[i].name);
        if (snprintf(to, sizeof to, "%s/%s", dest, list[i].name) >= (int)sizeof to) {
            job->failed = 1;
            fail("use the name", list[i].name, ENAMETOOLONG);
            break;
        }
        if (list[i].is_dir) {
            if (!iso_seen(j, list[i].lba)) copy_iso_dir(j, from, to, depth + 1);
        } else if (!copy_iso_file(j->iso, &list[i], to, j)) {
            job->failed = 1;
        } else if (job->progress != NULL && !job->progress(job->user, job->bytes)) {
            job->failed = 1;
            fail("finish", from, 0);   /* cancelled */
        }
    }
    free(list);
}

int64_t data_copy_iso_size(const char *iso_path, const char *root)
{
    IsoJob j = { { NULL, NULL, 0, 0 }, iso_open(iso_path, NULL, 0), { 0 }, 0 };
    if (j.iso == NULL) return -1;
    IsoEntry top;
    if (iso_stat(j.iso, root != NULL ? root : "", &top)) iso_seen(&j, top.lba);
    const int64_t size = iso_size_of(&j, root != NULL ? root : "", 0);
    iso_close(j.iso);
    return size;
}

int data_copy_iso(const char *iso_path, const char *root, const char *dest, DataCopyProgress progress, void *user)
{
    char part[1100], old[1100], why[160] = "";
    if (!begin_copy(dest, part, old)) return 0;
    IsoJob j = { { progress, user, 0, 0 }, iso_open(iso_path, why, sizeof why), { 0 }, 0 };
    if (j.iso == NULL) {
        snprintf(error_text, sizeof error_text, "could not read %s: %s", iso_path, why[0] != '\0' ? why : "not a CD image");
        port_log("data_copy: %s\n", error_text);
        return 0;
    }
    IsoEntry top;
    if (iso_stat(j.iso, root != NULL ? root : "", &top)) iso_seen(&j, top.lba);   /* a folder that points back at where the copy began is a loop too */
    copy_iso_dir(&j, root != NULL ? root : "", part, 0);
    iso_close(j.iso);
    return finish_copy(dest, part, old, &j.job);
}
