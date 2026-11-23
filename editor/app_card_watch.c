#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
/* The open car's card and files changed on disk by another program (app.h's app_card_*): the card is read again when it has no unsaved edits, else
 * kept with «Changed on disk»; the car file and the textures reach the lists, the Model and the Preview. See app.h. */
#include "app.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* the look at the car's files (mod.ini, the car file, the folders of textures): a directory read and a stat for each PNG, so about every 2.5 s like the masks' and the PSDs', never per frame (the workspace may be on a network folder) */
#define CARD_WATCH_EVERY 2.5

static int same(MaskStamp x, MaskStamp y)
{
    return x.mtime_ns == y.mtime_ns && x.size == y.size;
}

static void card_path(const App *a, char *out, size_t cap)
{
    snprintf(out, cap, "%s", a->edit.path);
}

static unsigned long long mix(unsigned long long h, unsigned long long v)
{
    return (h ^ v) * 1099511628211ULL;
}

/* a sum over the PNG files of modelo/<bank>: name, time and size of each (a sum, so the order of the listing does not matter) */
static unsigned long long pngs_of(const App *a)
{
    static const char *const BANKS[] = { "near", "far" };
    unsigned long long sum = 0;
    for (size_t b = 0; b < sizeof BANKS / sizeof BANKS[0]; b++) {
        char dir[CAR_PROJECT_PATH + 16];
        const int n = snprintf(dir, sizeof dir, "%s/model/%s/bank", a->car.root, BANKS[b]);
        DIR *d = n > 0 && (size_t)n < sizeof dir ? opendir(dir) : NULL;
        if (d == NULL) continue;
        for (const struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
            const size_t len = strlen(e->d_name);
            if (len < 5 || strcmp(e->d_name + len - 4, ".png") != 0) continue;
            char path[CAR_PROJECT_PATH + 80];
            if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path) continue;
            const MaskStamp s = app_file_stamp(path);
            unsigned long long h = 14695981039346656037ULL;
            for (const char *c = path; *c != '\0'; c++) h = mix(h, (unsigned char)*c);
            sum += mix(mix(h, (unsigned long long)s.mtime_ns), (unsigned long long)s.size);
        }
        closedir(d);
    }
    return sum;
}

void app_card_arm(App *a)
{
    CarDisk *d = &a->cdisk;
    char card[4200];
    card_path(a, card, sizeof card);
    d->card = a->car.masks_only ? (MaskStamp){ 0, 0 } : app_file_stamp(card);
    d->cfm = app_file_stamp(a->car.cfm);
    d->pngs = pngs_of(a);
    d->card_changed = 0;
}

/* the card read again into the editor; 0 (status names why) when the file is not a card the editor takes yet */
static int card_read(App *a, MaskStamp before)
{
    CarEdit next;
    char why[APP_MSG], card[4200];
    card_path(a, card, sizeof card);
    if (!car_edit_open(&next, card, why, sizeof why)) return 0;
    car_edit_free(&a->edit);
    a->edit = next;
    a->cdisk.card = before;
    a->cdisk.card_changed = 0;
    return 1;
}

static int card_look(App *a)
{
    CarDisk *d = &a->cdisk;
    if (a->car.masks_only || d->card_changed) return 0;   /* a masks project has no card; said already */
    char card[4200];
    card_path(a, card, sizeof card);
    const MaskStamp s = app_file_stamp(card);
    if (same(s, d->card)) return 0;
    if (car_edit_modified(&a->edit)) {
        d->card_changed = 1;   /* d->card stays the old stamp: Save keeps asking until Keep mine */
        snprintf(a->status, sizeof a->status, "mod.ini changed on disk: Reload, or Keep mine");
        return 1;
    }
    if (s.size == 0 || !card_read(a, s)) return 0;   /* gone, or half written: the open card stays, tried again at the next look */
    snprintf(a->status, sizeof a->status, "mod.ini changed on disk: read again");
    return 1;
}

/* the lists of the car read again (its project, then the textures and masks of the car file) */
static void refresh(App *a, int cfm_changed)
{
    CarProject *fresh = malloc(sizeof *fresh);
    char why[APP_MSG];
    if (fresh != NULL && car_project_detect(a->dir, fresh, why, sizeof why)) a->car = *fresh;
    free(fresh);
    if (!cfm_changed) return;
    char open_file[32] = "";
    if (a->mask.sel >= 0) snprintf(open_file, sizeof open_file, "%s", a->mask.tex[a->mask.sel].file);
    app_mask_scan(a);   /* closes the open mask: opened again below by its file's name */
    for (int i = 0; open_file[0] != '\0' && i < a->mask.tex_count; i++)
        if (strcmp(a->mask.tex[i].file, open_file) == 0) {
            app_mask_open(a, i);
            break;
        }
    a->cdisk.serial++;
    a->mask.generation++;
}

static int files_look(App *a)
{
    CarDisk *d = &a->cdisk;
    const MaskStamp cfm = app_file_stamp(a->car.cfm);
    const unsigned long long pngs = pngs_of(a);
    const int cfm_changed = !same(cfm, d->cfm);
    if (!cfm_changed && pngs == d->pngs) return 0;
    d->cfm = cfm;
    d->pngs = pngs;
    refresh(a, cfm_changed);
    snprintf(a->status, sizeof a->status, cfm_changed ? "the car file changed on disk: read again" : "the textures changed on disk: the list is read again");
    return 1;
}

int app_card_watch(App *a, double now)
{
    CarDisk *d = &a->cdisk;
    if (a->kind != APP_KIND_CAR) return 0;
    if (d->looked <= 0) {   // the first call: the card was armed when the car opened, the first look is CARD_WATCH_EVERY seconds on, as the other watches do
        d->looked = now > 0 ? now : 1e-3;
        return 0;
    }
    if (now - d->looked < CARD_WATCH_EVERY) return 0;
    d->looked = now;
    const int card = card_look(a);
    const int files = files_look(a);
    return card || files;
}

int app_card_reload(App *a)
{
    if (a->kind != APP_KIND_CAR || a->car.masks_only) return 1;
    char card[4200];
    card_path(a, card, sizeof card);
    const MaskStamp before = app_file_stamp(card);   /* taken before the read: a write during it is seen at the next look */
    if (!card_read(a, before)) {
        snprintf(a->status, sizeof a->status, "mod.ini cannot be read again: it is not a card the editor takes");
        return 0;
    }
    snprintf(a->status, sizeof a->status, "mod.ini read again: the edits are gone");
    return 1;
}

void app_card_keep_mine(App *a)
{
    char card[4200];
    card_path(a, card, sizeof card);
    a->cdisk.card = app_file_stamp(card);
    a->cdisk.card_changed = 0;
}

int app_card_save_ok(App *a)
{
    CarDisk *d = &a->cdisk;
    char card[4200];
    card_path(a, card, sizeof card);
    if (!d->card_changed && !same(app_file_stamp(card), d->card)) d->card_changed = 1;   /* changed since the last look */
    if (!d->card_changed) return 1;
    snprintf(a->status, sizeof a->status, "mod.ini changed on disk: Reload it, or Keep mine and Save again");
    return 0;
}
