#define _POSIX_C_SOURCE 200809L   /* st_mtim on glibc under -std=c99 */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE          /* st_mtimespec, which _POSIX_C_SOURCE alone hides */
#endif
/* The masks window's state for the open car (app.h's app_mask_*): the textures of its CFM, the mask shown over its texture, the files watched. */
#include "app.h"
#include "texture_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static void masks_path(const App *a, const char *file, char *out, size_t cap)
{
    char dir[APP_PATH];
    car_project_masks_dir(&a->car, dir, sizeof dir);
    snprintf(out, cap, "%s/%s", dir, file);
}

/* the file's time and size now; zeros when there is none */
static MaskStamp stamp_of(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return (MaskStamp){ 0, 0 };
#if defined(__APPLE__)
    const long long ns = (long long)st.st_mtimespec.tv_sec * 1000000000LL + st.st_mtimespec.tv_nsec;
#else
    const long long ns = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
#endif
    return (MaskStamp){ ns, (long long)st.st_size + 1 };   /* +1: an empty file is not "no file" */
}

MaskStamp app_file_stamp(const char *path)
{
    return stamp_of(path);
}

static MaskStamp stamp_at(const App *a, int index)
{
    char p[ME_PATH];
    masks_path(a, a->mask.tex[index].file, p, sizeof p);
    return stamp_of(p);
}

static int same(MaskStamp x, MaskStamp y)
{
    return x.mtime_ns == y.mtime_ns && x.size == y.size;
}

static void defaults(MaskUi *m)
{
    m->sel = -1;
    m->show = 1;
    for (int i = 0; i < ME_CLASSES; i++) m->class_on[i] = 1;
    m->opacity = 0.55f;
    m->zoom = 0;
}

void app_mask_close(App *a)
{
    a->mask.generation++;
    mask_edit_free(&a->mask.me);
    a->mask.sel = -1;
    a->mask.note[0] = '\0';
    a->mask.tex_file[0] = '\0';
}

void app_mask_scan(App *a)
{
    MaskUi *m = &a->mask;
    app_mask_close(a);
    const int first = m->opacity == 0;   /* nothing set yet: the tools' starting values */
    if (first) defaults(m);
    m->tex_count = 0;
    m->error[0] = '\0';
    if (a->kind != APP_KIND_CAR || a->car.cfm[0] == '\0') return;
    m->tex_count = mask_tex_list(a->car.cfm, m->tex, MASK_TEX_LIST);
    for (int i = 0; i < m->tex_count; i++) {
        char p[ME_PATH];
        masks_path(a, m->tex[i].file, p, sizeof p);
        m->has_mask[i] = access(p, R_OK) == 0;
        m->disk[i] = stamp_of(p);
    }
}

int app_mask_open(App *a, int index)
{
    MaskUi *m = &a->mask;
    if (index < 0 || index >= m->tex_count) return 0;
    app_mask_close(a);
    m->error[0] = '\0';
    if (!m->has_mask[index]) {
        char rel[CAR_PROJECT_NAME + 16];
        car_project_masks_rel(&a->car, rel, sizeof rel);
        snprintf(m->error, sizeof m->error, "%s has no mask in %s: the game uses its family rules for it", m->tex[index].name, rel);
        return 0;
    }
    char p[ME_PATH], why[APP_MSG];
    uint8_t *rgb = NULL;
    int w = 0, h = 0;
    if (!mask_tex_rgb(a->car.cfm, m->tex[index].level, m->tex[index].name, &w, &h, &rgb)) {
        w = h = 0;
        rgb = NULL;   /* the mask opens without a picture under it */
    }
    masks_path(a, m->tex[index].file, p, sizeof p);
    const MaskStamp before = stamp_of(p);   /* taken before the read: a write during it is seen at the next look */
    const int ok = mask_edit_open(&m->me, p, w, h, rgb, why, sizeof why);
    free(rgb);
    if (!ok) {
        snprintf(m->error, sizeof m->error, "%s", why);
        return 0;
    }
    m->sel = index;
    m->disk[index] = before;
    m->generation++;
    if (!mask_edit_size_note(&m->me, m->note, sizeof m->note)) m->note[0] = '\0';
    texture_file_of(a->car.root, m->tex[index].level, m->tex[index].name, m->tex_file, sizeof m->tex_file);
    snprintf(a->status, sizeof a->status, "showing %s over its texture", m->tex[index].file);
    return 1;
}

int app_mask_open_by_name(App *a, const char *name)
{
    char want[MASK_TEX_NAME + 8];
    for (int i = 0; i < a->mask.tex_count; i++) {
        snprintf(want, sizeof want, "%s", a->mask.tex[i].file);
        char *dot = strstr(want, "_mask.png");
        if (dot != NULL) *dot = '\0';
        if (strcmp(want, name) == 0) return app_mask_open(a, i);
    }
    snprintf(a->mask.error, sizeof a->mask.error, "the car file has no texture %s", name);
    return 0;
}

/* the open mask's file changed: read again, or closed when it went; 0 when the file cannot be read yet */
static int open_changed(App *a, MaskStamp now)
{
    MaskUi *m = &a->mask;
    const int index = m->sel;
    if (now.size == 0) {
        app_mask_close(a);
        snprintf(a->status, sizeof a->status, "%s changed on disk: it is gone", m->tex[index].file);
        return 1;
    }
    char p[ME_PATH], why[APP_MSG];
    MaskEdit probe = { 0 };
    masks_path(a, m->tex[index].file, p, sizeof p);
    if (!mask_edit_open(&probe, p, 0, 0, NULL, why, sizeof why)) return 0;   /* half written, or not a mask: the open one stays, tried again at the next look */
    mask_edit_free(&probe);
    if (app_mask_open(a, index)) snprintf(a->status, sizeof a->status, "%s changed on disk: read again", m->tex[index].file);
    return 1;
}

int app_mask_watch(App *a, double now)
{
    MaskUi *m = &a->mask;
    if (a->kind != APP_KIND_CAR || (m->disk_looked > 0 && now - m->disk_looked < 1.0)) return 0;
    m->disk_looked = now;
    int changed = 0;
    for (int i = 0; i < m->tex_count; i++) {
        const MaskStamp s = stamp_at(a, i);
        if (same(s, m->disk[i])) continue;
        if (i == m->sel) {
            if (!open_changed(a, s)) continue;   /* not readable yet */
            m->has_mask[i] = s.size != 0;
            changed = 1;
            continue;
        }
        m->disk[i] = s;
        m->has_mask[i] = s.size != 0;
        changed = 1;
    }
    if (changed) m->generation++;
    return changed;
}

/* a car's history is the card's steps (edit_seq.h): Undo and Redo walk them */
static unsigned long card_undo(const App *a) { return car_edit_undo_seq(&a->edit); }

int app_can_undo(const App *a)
{
    if (a->kind == APP_KIND_TRACK) return track_undo_can_undo(&a->track_undo, &a->project, &a->trace);
    return card_undo(a) != 0;
}

int app_can_redo(const App *a)
{
    if (a->kind == APP_KIND_TRACK) return track_undo_can_redo(&a->track_undo);
    return car_edit_redo_seq(&a->edit) != 0;
}

int app_undo(App *a)
{
    if (a->kind == APP_KIND_TRACK) return app_track_undo(a, 0);
    return car_edit_undo(&a->edit);
}

int app_redo(App *a)
{
    if (a->kind == APP_KIND_TRACK) return app_track_undo(a, 1);
    return car_edit_redo(&a->edit);
}

const char *app_mask_dash_state(const App *a)
{
    if (a->stale_ok)
        for (int i = 0; i < a->stale.count; ++i) {
            const StaleItem *it = &a->stale.item[i];
            if (it->dash) return it->edited ? "edited by hand" : it->state == STALE_OK ? "in sync" : it->state == STALE_NEWER ? "PSD newer" : "PSD found, no mask yet";
        }
    return NULL;
}

const char *app_mask_state(const App *a, int index)
{
    if (index < 0 || index >= a->mask.tex_count) return "no mask";
    const MaskTexInfo *t = &a->mask.tex[index];
    if (a->stale_ok)
        for (int i = 0; i < a->stale.count; ++i) {
            const StaleItem *it = &a->stale.item[i];
            if (it->dash || strcasecmp(it->png, t->file) != 0) continue;
            return it->edited ? "edited by hand" : it->state == STALE_OK ? "in sync" : it->state == STALE_NEWER ? "PSD newer" : "PSD found, no mask yet";
        }
    return a->mask.has_mask[index] ? "PNG only" : "no mask";
}

int app_reveal_masks(App *a)
{
    char dir[APP_PATH];
    struct stat st;
    if (a->kind != APP_KIND_CAR) return 0;
    car_project_edit_dir(&a->car, dir, sizeof dir);
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(a->status, sizeof a->status, "%s is not there yet: the masks are painted in PSDs kept in art-source/masks", dir);
        return 0;
    }
    snprintf(a->reveal, sizeof a->reveal, "%s", dir);
    return 1;
}
