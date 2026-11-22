/* See track_mask_make_psd.h. */
#include "track_mask_make_psd.h"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "formats/png.h"
#include "proc_run.h"

extern char **environ;

enum { MP_PATH = 1024, MP_WAIT_SECONDS = 30 };

__attribute__((format(printf, 3, 4))) static int say(char *why, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(why, cap, fmt, ap);
    va_end(ap);
    return 0;
}

static void remove_tree(const char *dir)   // the scratch folder is ours: its files, its stem folder, itself (two levels, no recursion into anything else)
{
    DIR *d = opendir(dir);
    if (d == NULL) return;
    for (const struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char path[MP_PATH];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (unlink(path) != 0) remove_tree(path);
    }
    closedir(d);
    rmdir(dir);
}

static const char *last_line(const char *log, char *out, size_t cap)
{
    size_t end = strlen(log);
    while (end > 0 && (log[end - 1] == '\n' || log[end - 1] == '\r')) --end;
    size_t start = end;
    while (start > 0 && log[start - 1] != '\n') --start;
    snprintf(out, cap, "%.*s", (int)(end - start), log + start);
    return out;
}

static int write_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    const int ok = f != NULL && fwrite(data, 1, len, f) == len;
    if (f != NULL && fclose(f) != 0) return 0;
    return ok;
}

static int mkdirs(const char *path)   // path and its parents, as mkdir -p
{
    char tmp[MP_PATH];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p != '\0'; ++p)
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return 0;
            *p = '/';
        }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

int track_mask_make_psd(const char *python, const char *tools, const char *root, const char *stem, const char *name, const uint8_t *rgba, int w, int h, char *why, size_t why_cap)
{
    char script[MP_PATH], dest[MP_PATH], dest_dir[MP_PATH], scratch[MP_PATH], drafts[MP_PATH], out[MP_PATH], png_path[MP_PATH], made[MP_PATH];
    if (tools[0] == '\0') return say(why, why_cap, "Create PSD needs the repository's tools folder (track_mask_psd.py): not found beside this program");
    snprintf(script, sizeof script, "%s/track_mask_psd.py", tools);
    if (access(script, R_OK) != 0) return say(why, why_cap, "%s is not there: Create PSD needs the repository's tools folder", script);
    if (rgba == NULL || w <= 0 || h <= 0) return say(why, why_cap, "%s has no pixels to make a PSD from", name);
    snprintf(dest_dir, sizeof dest_dir, "%s/art-source/masks", root);
    snprintf(dest, sizeof dest, "%s/%s.psd", dest_dir, name);
    if (access(dest, F_OK) == 0) return say(why, why_cap, "%s.psd is already in art-source/masks: Studio does not overwrite a PSD", name);
    const char *tmp = getenv("TMPDIR");
    snprintf(scratch, sizeof scratch, "%s%smaskpsd_XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp", tmp != NULL && tmp[0] != '\0' && tmp[strlen(tmp) - 1] != '/' ? "/" : "");
    if (mkdtemp(scratch) == NULL) return say(why, why_cap, "the scratch folder could not be made: %s", strerror(errno));
    snprintf(drafts, sizeof drafts, "%s/drafts/%s", scratch, stem);
    snprintf(out, sizeof out, "%s/out", scratch);
    snprintf(png_path, sizeof png_path, "%s/%s.rgb.png", drafts, name);
    snprintf(made, sizeof made, "%s/%s/%s.psd", out, stem, name);
    uint8_t *png = NULL;
    size_t len = 0;
    char *argv[7];
    char drafts_root[MP_PATH];
    snprintf(drafts_root, sizeof drafts_root, "%s/drafts", scratch);
    int ok = mkdirs(drafts) && mkdirs(out);
    if (!ok) say(why, why_cap, "the scratch folder %s could not be filled", scratch);
    if (ok && png_write_rgba(rgba, (uint32_t)w, (uint32_t)h, &png, &len) != PNG_OK) ok = say(why, why_cap, "the texture %s could not be written as a PNG", name);
    if (ok && !write_file(png_path, png, len)) ok = say(why, why_cap, "%s could not be written", png_path);
    free(png);
    ProcRun run;
    memset(&run, 0, sizeof run);
    if (ok) {
        int n = 0;
        if (python[0] == '\0') argv[n++] = "/usr/bin/env", argv[n++] = "python3";
        else argv[n++] = (char *)python;
        argv[n++] = script, argv[n++] = drafts_root, argv[n++] = out, argv[n++] = "--all", argv[n] = NULL;   // --all: the script writes the texture asked for, candidate or not
        char reason[200] = "";
        if (!proc_start(&run, argv, environ, NULL, reason, sizeof reason)) ok = say(why, why_cap, "python3 did not start: %s", reason);
    }
    if (ok) {
        const time_t t0 = time(NULL);
        while (proc_poll(&run) && time(NULL) - t0 < MP_WAIT_SECONDS) usleep(20000);
        if (run.running) proc_stop(&run), ok = say(why, why_cap, "track_mask_psd.py took more than %s seconds and was stopped", "30");
        else if (run.exit_code != 0 || access(made, R_OK) != 0) {
            char line[240];
            const char *log = run.log != NULL ? run.log : "";
            if (strstr(log, "No module named") != NULL) ok = say(why, why_cap, "python3 has no Pillow: run  pip3 install Pillow  and try again");
            else ok = say(why, why_cap, "track_mask_psd.py failed: %s", last_line(log, line, sizeof line));
        }
    }
    proc_free(&run);
    if (ok && (!mkdirs(dest_dir) || rename(made, dest) != 0)) ok = say(why, why_cap, "%s could not be put in art-source/masks", name);
    remove_tree(scratch);
    if (ok) why[0] = '\0';
    return ok;
}
