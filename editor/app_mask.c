#define _POSIX_C_SOURCE 200809L   /* st_mtim on glibc under -std=c99 */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE          /* st_mtimespec, which _POSIX_C_SOURCE alone hides */
#endif
/* The mask editor of the open car (app.h's app_mask_*): the textures of its CFM, the mask being edited, Save. */
#include "app.h"
#include "texture_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void say(App *a, const char *text)
{
    snprintf(a->status, sizeof a->status, "%s", text);
}

static void masks_path(const App *a, const char *file, char *out, size_t cap)
{
    snprintf(out, cap, "%s/car_masks/%s/%s", a->car.piece, a->car.folder, file);
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
    m->tool = MASK_TOOL_PENCIL;
    m->brush = 2;
    m->cls = 0;
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
    a->mask.disk_changed = 0;
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
    if (mask_edit_modified(&m->me)) {
        snprintf(m->error, sizeof m->error, "The open mask has unsaved changes: Save or Discard them first");
        return 0;
    }
    app_mask_close(a);
    m->error[0] = '\0';
    if (!m->has_mask[index]) {
        snprintf(m->error, sizeof m->error, "%s has no mask in car_masks/%s: the game uses its family rules for it", m->tex[index].name, a->car.folder);
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
    snprintf(a->status, sizeof a->status, "editing %s: Save writes this mask only", m->tex[index].file);
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

int app_mask_save(App *a)
{
    MaskUi *m = &a->mask;
    if (m->sel < 0) return 1;
    char why[APP_MSG];
    const int changed = mask_edit_modified(&m->me);
    if (changed && !m->disk_changed && !same(stamp_at(a, m->sel), m->disk[m->sel])) m->disk_changed = 1;   /* changed since the last look */
    if (m->disk_changed) {
        snprintf(m->error, sizeof m->error, "%s changed on disk: Reload it, or Keep mine and Save again", m->tex[m->sel].file);
        say(a, m->error);
        return 0;
    }
    if (!mask_edit_save(&m->me, why, sizeof why)) {
        snprintf(m->error, sizeof m->error, "%s", why);
        say(a, why);
        return 0;
    }
    m->error[0] = '\0';
    m->disk[m->sel] = stamp_at(a, m->sel);   /* our own write is not a change on disk */
    m->has_mask[m->sel] = 1;                 /* written back after it went (Keep mine) */
    m->generation++;
    if (changed) m->saved_since_race = 1;
    snprintf(a->status, sizeof a->status, changed ? "saved %s" : "nothing to save: %s is as it was", m->tex[m->sel].file);
    return 1;
}

int app_mask_modified(const App *a)
{
    return a->mask.sel >= 0 && mask_edit_modified((MaskEdit *)&a->mask.me);
}

/* the open mask's file changed: read again, closed when it went, or (unsaved edits) kept with the state line; 0 when the file cannot be read yet */
static int open_changed(App *a, MaskStamp now)
{
    MaskUi *m = &a->mask;
    const int index = m->sel;
    if (app_mask_modified(a)) {
        m->disk_changed = 1;   /* m->disk[index] stays the old one: Save keeps asking until Keep mine */
        snprintf(a->status, sizeof a->status, "%s changed on disk: Reload, or Keep mine", m->tex[index].file);
        return 1;
    }
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
            if (m->disk_changed || !open_changed(a, s)) continue;   /* said already; or not readable yet */
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

int app_mask_reload(App *a)
{
    MaskUi *m = &a->mask;
    const int index = m->sel;
    if (index < 0) return 1;
    mask_edit_free(&m->me);   /* the edits go: what app_mask_open refuses to drop */
    m->me = (MaskEdit){ 0 };
    m->disk_changed = 0;
    if (stamp_at(a, index).size == 0) {
        m->has_mask[index] = 0;
        app_mask_close(a);
        return 1;
    }
    return app_mask_open(a, index);
}

void app_mask_keep_mine(App *a)
{
    MaskUi *m = &a->mask;
    if (m->sel < 0) return;
    m->disk[m->sel] = stamp_at(a, m->sel);
    m->disk_changed = 0;
    m->error[0] = '\0';
}

/* a car's history is the card's steps and the open mask's in the order they were made (edit_seq.h): Undo takes the newest of the two stacks'
 * tops; Redo the one undone last, the older of the two redo tops, while nothing was edited since (a newer step on an undo stack) */
static unsigned long card_undo(const App *a) { return car_edit_undo_seq(&a->edit); }
static unsigned long mask_undo(const App *a) { return a->mask.sel >= 0 ? mask_edit_undo_seq(&a->mask.me) : 0; }

/* 1 the card's redo, 2 the mask's, 0 none */
static int redo_of(const App *a)
{
    const unsigned long card = car_edit_redo_seq(&a->edit), mask = a->mask.sel >= 0 ? mask_edit_redo_seq(&a->mask.me) : 0;
    const unsigned long newest_undo = card_undo(a) > mask_undo(a) ? card_undo(a) : mask_undo(a);
    const int which = card != 0 && (mask == 0 || card < mask) ? 1 : mask != 0 ? 2 : 0;
    const unsigned long seq = which == 1 ? card : which == 2 ? mask : 0;
    return seq > newest_undo ? which : 0;
}

int app_can_undo(const App *a)
{
    if (a->kind == APP_KIND_TRACK) return track_undo_can_undo(&a->track_undo, &a->project, &a->trace);
    return card_undo(a) != 0 || mask_undo(a) != 0;
}

int app_can_redo(const App *a)
{
    if (a->kind == APP_KIND_TRACK) return track_undo_can_redo(&a->track_undo);
    return redo_of(a) != 0;
}

int app_undo(App *a)
{
    if (a->kind == APP_KIND_TRACK) return app_track_undo(a, 0);
    if (mask_undo(a) > card_undo(a)) return mask_edit_undo(&a->mask.me);
    return car_edit_undo(&a->edit);
}

int app_redo(App *a)
{
    if (a->kind == APP_KIND_TRACK) return app_track_undo(a, 1);
    const int which = redo_of(a);
    return which == 1 ? car_edit_redo(&a->edit) : which == 2 ? mask_edit_redo(&a->mask.me) : 0;
}
