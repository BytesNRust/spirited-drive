/* Convert PSD in the background (psd_job.h). */
#include "psd_job.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

extern char **environ;

static const char BODY_SCRIPT[] = "car_mask_from_psd.py", DASH_SCRIPT[] = "dash_mask_from_psd.py";

void psd_job_init(PsdJob *job)
{
    memset(job, 0, sizeof *job);
    job->run.fd = -1;
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int join(char *out, size_t cap, const char *a, const char *b)
{
    const int n = snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
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

static int run_script(PsdJob *job, const char *script, const char *out_dir, char *why, size_t why_cap)
{
    char path[PSD_PATH];
    if (!join(path, sizeof path, job->tools, script)) return snprintf(why, why_cap, "the tools path is too long"), 0;
    if (access(path, R_OK) != 0) return snprintf(why, why_cap, "%s is not in %s: Convert PSD needs the repository's tools folder", script, job->tools), 0;
    char *argv[] = { "/usr/bin/env", "python3", path, job->psd_dir, (char *)out_dir, NULL };
    proc_free(&job->run);
    return proc_start(&job->run, argv, environ, NULL, why, why_cap);
}

static void remove_files_in(const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL) return;
    for (const struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        char path[PSD_PATH];
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || !join(path, sizeof path, dir, e->d_name)) continue;
        if (is_dir(path)) remove_files_in(path), rmdir(path);
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
    char tmp[PSD_PATH + 8];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", to) >= (int)sizeof tmp) return 0;
    FILE *in = fopen(from, "rb"), *out = in != NULL ? fopen(tmp, "wb") : NULL;
    int ok = in != NULL && out != NULL;
    char buf[8192];
    size_t n;
    while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0) ok = fwrite(buf, 1, n, out) == n;
    if (ok) ok = !ferror(in);
    if (in != NULL) fclose(in);
    if (out != NULL) ok = fclose(out) == 0 && ok;
    if (ok) ok = rename(tmp, to) == 0;
    if (!ok) remove(tmp);
    return ok;
}

/* every PNG the script wrote below `from` (one folder a car, PNG files) into <piece>/<kind>/<folder>/; the count, -1 on a failed write */
static int copy_pngs(const PsdJob *job, const char *from, const char *kind)
{
    char top[PSD_PATH], dest[PSD_PATH];
    if (!join(top, sizeof top, job->piece, kind) || !join(dest, sizeof dest, top, job->folder)) return -1;
    DIR *d = opendir(from);
    if (d == NULL) return 0;
    int count = 0;
    for (const struct dirent *e = readdir(d); e != NULL && count >= 0; e = readdir(d)) {
        char sub[PSD_PATH];
        if (e->d_name[0] == '.' || !join(sub, sizeof sub, from, e->d_name) || !is_dir(sub)) continue;
        DIR *s = opendir(sub);
        for (const struct dirent *f = s != NULL ? readdir(s) : NULL; f != NULL && count >= 0; f = readdir(s)) {
            const size_t len = strlen(f->d_name);
            char src[PSD_PATH], to[PSD_PATH];
            if (len < 5 || strcmp(f->d_name + len - 4, ".png") != 0 || !join(src, sizeof src, sub, f->d_name)) continue;
            if ((mkdir(top, 0755) != 0 && errno != EEXIST) || (mkdir(dest, 0755) != 0 && errno != EEXIST) || !join(to, sizeof to, dest, f->d_name) || !copy_file(src, to)) count = -1;
            else count++;
        }
        if (s != NULL) closedir(s);
    }
    closedir(d);
    return count;
}

static void fail(PsdJob *job, const char *what)
{
    const char *log = job->run.log != NULL ? job->run.log : "";
    const size_t len = strlen(log), keep = len > 160 ? 160 : len;
    char tail[200];
    snprintf(tail, sizeof tail, "%s", log + len - keep);
    for (char *c = tail; *c != '\0'; c++) if (*c == '\n') *c = ' ';
    snprintf(job->text, sizeof job->text, "Convert PSD failed: %s%s%s", what, tail[0] != '\0' ? ": " : "", tail);
    job->finished = 1, job->ok = 0, job->stage = 0;
    clean_scratch(job);
}

static void finish(PsdJob *job)
{
    char from[PSD_PATH];
    if (!join(from, sizeof from, job->scratch, "masks")) { fail(job, "a path is too long"); return; }
    job->masks = copy_pngs(job, from, "car_masks");
    if (job->masks < 0) { fail(job, "a mask could not be written into the project"); return; }
    if (job->masks == 0) {
        fail(job, "the script wrote no masks (does each PSD have a \"mask\" layer?)");
        return;
    }
    if (join(from, sizeof from, job->scratch, "dash")) job->dash_files = copy_pngs(job, from, "dash_masks");
    if (job->dash_files < 0) { fail(job, "a dash file could not be written into the project"); return; }
    snprintf(job->text, sizeof job->text, "Converted %d mask%s from %s%s.", job->masks, job->masks == 1 ? "" : "s", job->psd_dir, job->dash_files > 0 ? ", and the dash" : "");
    job->finished = 1, job->ok = 1, job->stage = 0;
    clean_scratch(job);
}

static int has_dash_psd(const PsdJob *job)
{
    char path[PSD_PATH];
    return join(path, sizeof path, job->psd_dir, "dash.psd") && access(path, R_OK) == 0;
}

int psd_job_start(PsdJob *job, const char *tools, const char *psd_dir, const char *folder, const char *piece, char *why, size_t why_cap)
{
    if (job->stage != 0 || job->run.running) return snprintf(why, why_cap, "a conversion is running already"), 0;
    if (strlen(tools) >= PSD_PATH || strlen(psd_dir) >= PSD_PATH || strlen(piece) >= PSD_PATH || strlen(folder) >= sizeof job->folder)
        return snprintf(why, why_cap, "a path is too long"), 0;
    psd_job_init(job);
    snprintf(job->tools, sizeof job->tools, "%s", tools);
    snprintf(job->psd_dir, sizeof job->psd_dir, "%s", psd_dir);
    snprintf(job->folder, sizeof job->folder, "%s", folder);
    snprintf(job->piece, sizeof job->piece, "%s", piece);
    const char *tmp = getenv("TMPDIR");
    if (snprintf(job->scratch, sizeof job->scratch, "%s/tnfs-psd-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp") >= (int)sizeof job->scratch || mkdtemp(job->scratch) == NULL)
        return job->scratch[0] = '\0', snprintf(why, why_cap, "no scratch folder for the conversion"), 0;
    char body[PSD_PATH];
    if (!join(body, sizeof body, job->scratch, "masks") || mkdir(body, 0755) != 0) return clean_scratch(job), snprintf(why, why_cap, "no scratch folder for the conversion"), 0;
    char dash[PSD_PATH];
    if (!join(dash, sizeof dash, job->scratch, "dash") || mkdir(dash, 0755) != 0) return clean_scratch(job), snprintf(why, why_cap, "no scratch folder for the conversion"), 0;
    job->stage = 1;
    if (!run_script(job, BODY_SCRIPT, body, why, why_cap)) {
        job->stage = 0;
        clean_scratch(job);
        return 0;
    }
    return 1;
}

int psd_job_poll(PsdJob *job)
{
    if (job->stage == 0) return 0;
    if (proc_poll(&job->run)) return 1;
    char why[200], out[PSD_PATH];
    if (job->run.stopped) return fail(job, "it was stopped"), 0;
    if (job->run.exit_code != 0) {
        snprintf(why, sizeof why, "%s stopped with status %d (nothing was copied into the project)", job->stage == 1 ? BODY_SCRIPT : DASH_SCRIPT, job->run.exit_code);
        return fail(job, why), 0;
    }
    if (job->stage == 1 && has_dash_psd(job)) {
        job->stage = 2;
        if (!join(out, sizeof out, job->scratch, "dash") || !run_script(job, DASH_SCRIPT, out, why, sizeof why)) return fail(job, why), 0;
        return 1;
    }
    finish(job);
    return 0;
}

int psd_job_busy(const PsdJob *job)
{
    return job->stage != 0;
}

void psd_job_stop(PsdJob *job)
{
    if (job->run.running) proc_stop(&job->run);
}

void psd_job_free(PsdJob *job)
{
    proc_free(&job->run);
    clean_scratch(job);
    job->stage = 0;
}
