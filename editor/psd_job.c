/* Convert PSD in the background (psd_job.h). */
#include "psd_job.h"

#include "psd_convert.h"
#include "psd_manifest.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

void psd_job_init(PsdJob *job)
{
    memset(job, 0, sizeof *job);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_real_dir(const char *path)   /* a symbolic link is not followed */
{
    struct stat st;
    return lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int join(char *out, size_t cap, const char *a, const char *b)
{
    const int n = snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
}

/* mkdir -p: 1 when the folder is there at the end */
static int make_dirs(const char *path)
{
    char part[PSD_PATH];
    if (strlen(path) >= sizeof part) return 0;
    snprintf(part, sizeof part, "%s", path);
    for (char *c = part + 1; *c != '\0'; ++c) {
        if (*c != '/') continue;
        *c = '\0';
        if (mkdir(part, 0755) != 0 && errno != EEXIST) return 0;
        *c = '/';
    }
    return (mkdir(part, 0755) == 0 || errno == EEXIST) && is_dir(part);
}

int psd_job_find(const char *psd_root, const char *folder, char *out, size_t cap)
{
    DIR *d = psd_root != NULL && folder != NULL ? opendir(psd_root) : NULL;
    if (d == NULL) return 0;
    int found = 0;
    for (const struct dirent *e = readdir(d); e != NULL && !found; e = readdir(d)) {
        char path[PSD_PATH];
        if (e->d_name[0] == '.' || strcasecmp(e->d_name, folder) != 0 || !join(path, sizeof path, psd_root, e->d_name) || !is_dir(path)) continue;
        found = join(out, cap, psd_root, e->d_name);
    }
    closedir(d);
    return found;
}

static void remove_files_in(const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL) return;
    for (const struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        char path[PSD_PATH];
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || !join(path, sizeof path, dir, e->d_name)) continue;
        if (is_real_dir(path)) remove_files_in(path), rmdir(path);
        else unlink(path);
    }
    closedir(d);
}

static void clean_scratch(PsdJob *job)
{
    if (job->scratch[0] == '\0') return;
    remove_files_in(job->scratch);
    rmdir(job->scratch);
    job->scratch[0] = '\0';
}

static int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb"), *out = in != NULL ? fopen(to, "wb") : NULL;
    int ok = in != NULL && out != NULL;
    char buf[8192];
    size_t n;
    while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0) ok = fwrite(buf, 1, n, out) == n;
    if (ok) ok = !ferror(in);
    if (in != NULL) fclose(in);
    if (out != NULL) ok = fclose(out) == 0 && ok;
    return ok;
}

/* a file written next to its place and not yet in it: all of a conversion's are made first, then all are renamed (a failed write leaves the project untouched) */
enum { STAGE_MAX = 256 };
typedef struct {
    char tmp[STAGE_MAX][PSD_PATH + 8], to[STAGE_MAX][PSD_PATH];
    int count;
} Staged;

static void unstage(Staged *st)
{
    for (int i = 0; i < st->count; i++) remove(st->tmp[i]);
    st->count = 0;
}

/* a PNG in `dest` Convert PSD must not replace: its hash is not the manifest's, or the manifest has no line for it (a kit from before the manifest) and it is not what the PSD gives (`src`, the scratch copy) */
static int kept_by_hand(const Manifest *known, const char *dest, const char *name, const char *src)
{
    const GenState state = manifest_state(known, dest, name);
    if (state == GEN_EDITED) return 1;
    if (state != GEN_UNKNOWN) return 0;
    char mine[MANIFEST_SHA], theirs[MANIFEST_SHA], path[PSD_PATH];
    if (!join(path, sizeof path, dest, name) || !manifest_file_sha(path, mine)) return 0;
    return !manifest_file_sha(src, theirs) || strcmp(mine, theirs) != 0;
}

/* every PNG written below `from` (one folder a car, PNG files) copied beside its place in `dest`; the count, -1 on a failure */
static int stage_pngs(PsdJob *job, const Manifest *known, const char *from, const char *dest, Staged *st)
{
    DIR *d = opendir(from);
    if (d == NULL) return 0;
    int count = 0;
    for (const struct dirent *e = readdir(d); e != NULL && count >= 0; e = readdir(d)) {
        char sub[PSD_PATH];
        if (e->d_name[0] == '.' || !join(sub, sizeof sub, from, e->d_name) || !is_real_dir(sub)) continue;
        DIR *s = opendir(sub);
        for (const struct dirent *f = s != NULL ? readdir(s) : NULL; f != NULL && count >= 0; f = readdir(s)) {
            const size_t len = strlen(f->d_name);
            char src[PSD_PATH];
            if (len < 5 || strcmp(f->d_name + len - 4, ".png") != 0 || !join(src, sizeof src, sub, f->d_name)) continue;
            if (!job->force && known != NULL && kept_by_hand(known, dest, f->d_name, src)) {   /* edited by hand since Studio made it, or a PNG of a project with no manifest line that is not what the PSD gives: kept */
                const size_t used = strlen(job->kept_names);
                job->kept++;
                if (used + strlen(f->d_name) + 3 < sizeof job->kept_names) snprintf(job->kept_names + used, sizeof job->kept_names - used, "%s%s", used > 0 ? ", " : "", f->d_name);
                continue;
            }
            const int slot = st->count;
            if (slot >= STAGE_MAX || !make_dirs(dest) || !join(st->to[slot], sizeof st->to[slot], dest, f->d_name) ||
                snprintf(st->tmp[slot], sizeof st->tmp[slot], "%s.psdtmp", st->to[slot]) >= (int)sizeof st->tmp[slot])
                count = -1;
            else {
                st->count++;
                if (copy_file(src, st->tmp[slot])) count++;
                else count = -1;
            }
        }
        if (s != NULL) closedir(s);
    }
    closedir(d);
    return count;
}

static void fail(PsdJob *job, const char *what)
{
    snprintf(job->text, sizeof job->text, "Convert PSD failed: %s", what);
    job->finished = 1, job->ok = 0;
    clean_scratch(job);
}

/* the reader wrote `name` for the dash in the scratch folder (<scratch>/dash/<folder>/) */
static int scratch_dash_has(const PsdJob *job, const char *name)
{
    char path[PSD_PATH];
    const int n = snprintf(path, sizeof path, "%s/dash/%s/%s", job->scratch, job->folder, name);
    return n > 0 && (size_t)n < sizeof path && access(path, F_OK) == 0;
}

static void finish(PsdJob *job)
{
    char from[PSD_PATH];
    Staged *st = calloc(1, sizeof *st);
    Manifest *manifest = job->manifest[0] != '\0' ? calloc(1, sizeof *manifest) : NULL;
    if (st == NULL || (job->manifest[0] != '\0' && manifest == NULL)) {
        free(st);
        free(manifest);
        fail(job, "out of memory");
        return;
    }
    int written = manifest == NULL || manifest_load(job->manifest, manifest);
    if (!join(from, sizeof from, job->scratch, "masks")) {
        fail(job, "a path is too long");
        free(st);
        free(manifest);
        return;
    }
    job->masks = stage_pngs(job, manifest, from, job->masks_dst, st);
    if (job->masks >= 0 && job->masks + job->kept > 0 && join(from, sizeof from, job->scratch, "dash")) job->dash_files = stage_pngs(job, manifest, from, job->dash_dst, st);
    if (job->masks < 0 || job->dash_files < 0) {
        unstage(st);
        free(st);
        free(manifest);
        fail(job, "a file could not be written into the project (nothing was changed)");
        return;
    }
    if (job->masks + job->kept == 0) {
        free(st);
        free(manifest);
        fail(job, "no PSD of the car was found in its PSD folder");
        return;
    }
    int renamed = 0;
    for (int i = 0; i < st->count; i++) renamed += rename(st->tmp[i], st->to[i]) == 0;
    if (renamed != st->count) {
        unstage(st);
        free(st);
        free(manifest);
        fail(job, "some files could not be put in place: the project may hold part of the new masks");
        return;
    }
    for (int i = 0; manifest != NULL && i < st->count; i++) {   /* what was made, hashed: a later edit of it by hand is told from it */
        const char *base = strrchr(st->to[i], '/');
        char sha[MANIFEST_SHA];
        written &= manifest_file_sha(st->to[i], sha) && manifest_set(manifest, base != NULL ? base + 1 : st->to[i], sha);
    }
    free(st);
    const int has_dash = scratch_dash_has(job, "dash_mask.png"), has_colour = scratch_dash_has(job, "dash_color.png");   /* what the reader made says whether the dash.psd has a colour layer, whatever was kept */
    if (has_dash && !has_colour) {   /* a dash.psd with no colour layer: the colour of an older conversion goes */
        char file[PSD_PATH];
        const GenState was = manifest != NULL ? manifest_state(manifest, job->dash_dst, "dash_color.png") : GEN_MISSING;
        const int by_hand = !job->force && (was == GEN_EDITED || was == GEN_UNKNOWN);   /* an edit of it is kept too, and one the manifest has no line for (nothing says Studio made it) */
        if (!by_hand && join(file, sizeof file, job->dash_dst, "dash_color.png")) unlink(file);
        if (!by_hand && manifest != NULL) manifest_forget(manifest, "dash_color.png");
    }
    if (manifest != NULL) written &= manifest_save(job->manifest, manifest);
    free(manifest);
    if (!written) {
        fail(job, "the masks were made, but their manifest (.generated.tsv) could not be written");
        return;
    }
    const int n = snprintf(job->text, sizeof job->text, "Converted %d mask%s from %s%s.", job->masks, job->masks == 1 ? "" : "s", job->psd_dir, job->dash_files > 0 ? ", and the dash" : "");
    if (job->kept > 0 && n > 0 && (size_t)n < sizeof job->text)
        snprintf(job->text + n, sizeof job->text - (size_t)n, " Kept as edited by hand: %s (Regenerate from PSD replaces %s).", job->kept_names, job->kept == 1 ? "it" : "them");
    job->finished = 1, job->ok = 1;
    clean_scratch(job);
}

/* the thread: every PSD of the folder into the scratch folder */
static void *convert_thread(void *arg)
{
    PsdJob *job = arg;
    char masks[PSD_PATH], dash[PSD_PATH], why[PSD_TEXT] = "";
    PsdConverted done = { 0 };
    int ok = join(masks, sizeof masks, job->scratch, "masks") && join(dash, sizeof dash, job->scratch, "dash") && mkdir(masks, 0755) == 0 && mkdir(dash, 0755) == 0;
    if (ok) ok = psd_convert(job->psd_dir, job->folder, masks, dash, &job->cancel, &done, why, sizeof why);
    else snprintf(why, sizeof why, "no scratch folder for the conversion");
    pthread_mutex_lock(&job->lock);
    job->conv_ok = ok;
    snprintf(job->conv_why, sizeof job->conv_why, "%s", why);
    job->running = 0;
    pthread_mutex_unlock(&job->lock);
    return NULL;
}

int psd_job_start(PsdJob *job, const char *psd_dir, const char *folder, const char *masks_dst, const char *dash_dst, const char *manifest, int force, char *why, size_t why_cap)
{
    if (job->started) return snprintf(why, why_cap, "a conversion is running already"), 0;
    if (strlen(psd_dir) >= PSD_PATH || strlen(masks_dst) >= PSD_PATH || strlen(dash_dst) >= PSD_PATH || strlen(manifest) >= PSD_PATH || strlen(folder) >= sizeof job->folder || folder[0] == '\0' || folder[0] == '.' || strchr(folder, '/') != NULL)
        return snprintf(why, why_cap, "a path is too long"), 0;
    if (job->ready) pthread_mutex_destroy(&job->lock);
    psd_job_init(job);
    pthread_mutex_init(&job->lock, NULL);
    job->ready = 1;
    snprintf(job->psd_dir, sizeof job->psd_dir, "%s", psd_dir);
    snprintf(job->folder, sizeof job->folder, "%s", folder);
    snprintf(job->masks_dst, sizeof job->masks_dst, "%s", masks_dst);
    snprintf(job->dash_dst, sizeof job->dash_dst, "%s", dash_dst);
    snprintf(job->manifest, sizeof job->manifest, "%s", manifest);
    job->force = force;
    const char *tmp = getenv("TMPDIR");
    if (snprintf(job->scratch, sizeof job->scratch, "%s/tnfs-psd-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp") >= (int)sizeof job->scratch || mkdtemp(job->scratch) == NULL)
        return job->scratch[0] = '\0', snprintf(why, why_cap, "no scratch folder for the conversion"), 0;
    job->running = 1;
    if (pthread_create(&job->thread, NULL, convert_thread, job) != 0) {
        job->running = 0;
        clean_scratch(job);
        return snprintf(why, why_cap, "the conversion could not start"), 0;
    }
    job->started = 1;
    return 1;
}

int psd_job_poll(PsdJob *job)
{
    if (!job->started) return 0;
    pthread_mutex_lock(&job->lock);
    const int running = job->running;
    pthread_mutex_unlock(&job->lock);
    if (running) return 1;
    pthread_join(job->thread, NULL);
    job->started = 0;
    if (!job->conv_ok) return fail(job, job->conv_why), 0;
    finish(job);
    return 0;
}

int psd_job_busy(const PsdJob *job)
{
    return job->started;
}

void psd_job_stop(PsdJob *job)
{
    if (!job->started) return;
    __atomic_store_n(&job->cancel, 1, __ATOMIC_RELAXED);
    pthread_join(job->thread, NULL);
    job->started = 0;
    snprintf(job->text, sizeof job->text, "Convert PSD failed: it was stopped");
    job->finished = 1, job->ok = 0;
    clean_scratch(job);
}

void psd_job_free(PsdJob *job)
{
    psd_job_stop(job);
    clean_scratch(job);
    if (job->ready) pthread_mutex_destroy(&job->lock);
    job->ready = 0;
}
