/* Check dash: see psd_stale.h. */
#include "psd_stale.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum { PATH = 4096 };

static int join(char *out, size_t cap, const char *a, const char *b)
{
    const int n = snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
}

static int is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

/* the modification time of the file in `dir` named `name` without case; 0 when none (a file time is never 0 in practice) */
static long find_mtime(const char *dir, const char *name, char *real, size_t real_cap)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    long when = 0;
    for (struct dirent *e = readdir(d); e != NULL && when == 0; e = readdir(d)) {
        char p[PATH];
        struct stat st;
        if (strcasecmp(e->d_name, name) != 0 || !join(p, sizeof p, dir, e->d_name) || stat(p, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        when = (long)st.st_mtime;
        if (real != NULL) snprintf(real, real_cap, "%s", e->d_name);
    }
    closedir(d);
    return when;
}

/* the dash mask of the project: dash_masks/<any folder>/dash_mask.png (a project holds one car's) */
static long dash_mtime(const char *piece)
{
    char dir[PATH];
    if (!join(dir, sizeof dir, piece, "dash_masks")) return 0;
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    long when = 0;
    for (struct dirent *e = readdir(d); e != NULL && when == 0; e = readdir(d)) {
        char sub[PATH];
        if (e->d_name[0] == '.' || !join(sub, sizeof sub, dir, e->d_name) || !is_dir(sub)) continue;
        when = find_mtime(sub, "dash_mask.png", NULL, 0);
    }
    closedir(d);
    return when;
}

static int add(StaleReport *r, const char *psd, const char *png, int dash, StaleState state)
{
    if (r->count >= STALE_MAX) return 0;
    StaleItem *i = &r->item[r->count++];
    snprintf(i->psd, sizeof i->psd, "%s", psd);
    snprintf(i->png, sizeof i->png, "%s", png);
    i->dash = dash;
    i->state = state;
    r->stale += state != STALE_OK;
    return 1;
}

int psd_stale_check(const char *psd_dir, const char *piece, const char *folder, int drivable, StaleReport *out)
{
    memset(out, 0, sizeof *out);
    DIR *d = opendir(psd_dir);
    if (d == NULL) return 0;
    char masks[PATH];
    const int have_masks = join(masks, sizeof masks, piece, "car_masks") && strlen(masks) + strlen(folder) + 2 < sizeof masks;
    char folder_path[PATH] = "";
    if (have_masks) join(folder_path, sizeof folder_path, masks, folder);
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        const char *dot = strrchr(e->d_name, '.');
        char stem[STALE_NAME], path[PATH];
        struct stat st;
        if (dot == NULL || strcasecmp(dot, ".psd") != 0 || e->d_name[0] == '.' || (size_t)(dot - e->d_name) >= sizeof stem) continue;
        snprintf(stem, sizeof stem, "%.*s", (int)(dot - e->d_name), e->d_name);
        if (strchr(stem, '.') != NULL || !join(path, sizeof path, psd_dir, e->d_name) || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;   // a backup: not a PSD of the car
        const int dash = strcasecmp(stem, "dash") == 0;
        if (dash && !drivable) continue;
        char png[STALE_NAME];
        long made;
        if (dash) {
            snprintf(png, sizeof png, "dash_mask.png");
            made = dash_mtime(piece);
        } else {
            snprintf(png, sizeof png, "%s_mask.png", stem);
            made = folder_path[0] != '\0' ? find_mtime(folder_path, png, NULL, 0) : 0;
        }
        add(out, e->d_name, png, dash, made == 0 ? STALE_MISSING : made < (long)st.st_mtime ? STALE_NEWER : STALE_OK);
    }
    closedir(d);
    return 1;
}

void psd_stale_text(const StaleReport *r, char *out, size_t cap)
{
    size_t used = 0;
    if (cap > 0) out[0] = '\0';
    for (int i = 0; i < r->count && used + 1 < cap; ++i) {
        const StaleItem *it = &r->item[i];
        if (it->state == STALE_OK) continue;
        used += (size_t)snprintf(out + used, cap - used, "%s%s %s %s", used > 0 ? "; " : "", it->psd, it->state == STALE_NEWER ? "is newer than" : "has no", it->png);
    }
}
