/* Convert PSD in the app (app.h's app_psd_*): the glue between a car project and psd_job.h. */
#include "app.h"

#include "formats/car_kit_paths.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static int join(char *out, size_t cap, const char *a, const char *b)
{
    const int n = snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
}

void app_psd_stop(App *a)
{
    psd_job_stop(&a->psd);
}

void app_psd_setup(App *a, const char *base, const char *root)
{
    (void)base;
    a->psd_root[0] = '\0';
    const char *home = getenv("HOME"), *chosen = root != NULL ? root : getenv("TNFS_PSD_ROOT");
    if (chosen != NULL && chosen[0] != '\0') snprintf(a->psd_root, sizeof a->psd_root, "%s", chosen);
    else if (home != NULL && home[0] != '\0') join(a->psd_root, sizeof a->psd_root, home, "workspace/bytes-n-rust/art/tnfs-masks");
}

static int holds_psd(const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int found = 0;
    for (const struct dirent *e = readdir(d); e != NULL && !found; e = readdir(d)) {
        const char *dot = strrchr(e->d_name, '.');
        found = e->d_name[0] != '.' && dot != NULL && strcasecmp(dot, ".psd") == 0;
    }
    closedir(d);
    return found;
}

int app_psd_psd_dir(const App *a, char *out, size_t cap)
{
    if (a->kind == APP_KIND_CAR && app_car_is_kit(a) && snprintf(out, cap, "%s/%s", a->car.root, car_kit_path(CAR_KIT_SRC_MASKS)) < (int)cap && holds_psd(out)) return 1;   /* a kit keeps its PSDs */
    return a->kind == APP_KIND_CAR && a->car.folder[0] != '\0' && a->psd_root[0] != '\0' && psd_job_find(a->psd_root, a->car.folder, out, cap);
}

int app_psd_busy(const App *a)
{
    return psd_job_busy(&a->psd);
}

void app_psd_dirs(const App *a, char *masks, char *dash, char *manifest, size_t cap)
{
    car_project_masks_dir(&a->car, masks, cap);
    car_project_dash_dir(&a->car, dash, cap);
    car_project_manifest(&a->car, manifest, cap);
}

static int start(App *a)
{
    char masks[APP_PATH], dash[APP_PATH], manifest[APP_PATH];
    char dir[APP_PATH], why[APP_MSG];
    if (a->kind != APP_KIND_CAR) return snprintf(a->status, sizeof a->status, "open a car first"), 0;
    if (psd_job_busy(&a->psd)) return snprintf(a->status, sizeof a->status, "Convert PSD is already running"), 0;
    if (!app_psd_psd_dir(a, dir, sizeof dir))
        return snprintf(a->status, sizeof a->status, "no PSD folder named %s in %s", a->car.folder, a->psd_root[0] != '\0' ? a->psd_root : "(no PSD folder set: --psd-root)"), 0;
    app_psd_dirs(a, masks, dash, manifest, sizeof masks);
    if (!psd_job_start(&a->psd, dir, a->car.folder, masks, dash, manifest, why, sizeof why)) return snprintf(a->status, sizeof a->status, "Convert PSD did not start: %s", why), 0;
    snprintf(a->status, sizeof a->status, "Converting the PSDs of %s: the masks are replaced when every PSD has been read", a->car.folder);
    return 1;
}

int app_psd_start(App *a)
{
    a->psd_auto_set = 0;   /* a person asking again: the automatic conversion may try this state too */
    return start(a);
}

/* FNV-1a over what makes a stale state: which PSDs, how they stand and when they were saved */
static unsigned stale_signature(const StaleReport *r)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < r->count; ++i) {
        const StaleItem *it = &r->item[i];
        if (it->state == STALE_OK) continue;
        for (const char *c = it->psd; *c != '\0'; ++c) h = (h ^ (unsigned char)*c) * 16777619u;
        h = (h ^ (unsigned)it->state) * 16777619u;
        h = (h ^ (unsigned)it->psd_time) * 16777619u;
    }
    return h;
}

int app_psd_auto(App *a, const StaleReport *r)
{
    if (r->stale == 0) return a->psd_auto_set = a->psd_auto_fail = 0;   /* all in sync: whatever is stale next is a new state */
    const unsigned sig = stale_signature(r);
    if (a->psd_auto_fail) {   /* a conversion failed (this one, or a click's): the state it failed on stays untried until a PSD is saved again or Convert PSD is pressed */
        a->psd_auto_fail = 0, a->psd_auto_set = 1, a->psd_auto_sig = sig;
        return 0;
    }
    if (mask_edit_modified(&a->mask.me)) return 0;   /* the conversion closes the open mask: paint that is not saved is not thrown away by itself (it goes on when it is saved) */
    if ((a->psd_auto_set && a->psd_auto_sig == sig) || a->dash_check || a->sheet_active || car_race_busy(&a->race) || psd_job_busy(&a->psd)) return 0;
    a->psd_auto_set = 1, a->psd_auto_sig = sig;   /* set before the start: a start that fails (no folder, no room) is not retried every few frames */
    if (!start(a)) return 0;
    snprintf(a->status, sizeof a->status, "A PSD of %s changed: converting the PSDs", a->car.folder);
    return 1;
}

int app_psd_poll(App *a)
{
    if (!psd_job_busy(&a->psd)) return 0;
    if (psd_job_poll(&a->psd)) return 1;
    snprintf(a->status, sizeof a->status, "%s", a->psd.text);
    a->psd_auto_fail = !a->psd.ok;
    if (a->psd.ok) {
        app_car_reread(a);   /* the lists, and the masks read again (a mask that was open is closed: it was just remade) */
    }
    return 0;
}
