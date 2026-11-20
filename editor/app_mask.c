/* The mask editor of the open car (app.h's app_mask_*): the textures of its CFM, the mask being edited, Save. */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void say(App *a, const char *text)
{
    snprintf(a->status, sizeof a->status, "%s", text);
}

static void masks_path(const App *a, const char *file, char *out, size_t cap)
{
    snprintf(out, cap, "%s/car_masks/%s/%s", a->car.piece, a->car.folder, file);
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
    const int ok = mask_edit_open(&m->me, p, w, h, rgb, why, sizeof why);
    free(rgb);
    if (!ok) {
        snprintf(m->error, sizeof m->error, "%s", why);
        return 0;
    }
    m->sel = index;
    m->generation++;
    if (!mask_edit_size_note(&m->me, m->note, sizeof m->note)) m->note[0] = '\0';
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
    if (!mask_edit_save(&m->me, why, sizeof why)) {
        snprintf(m->error, sizeof m->error, "%s", why);
        say(a, why);
        return 0;
    }
    m->error[0] = '\0';
    m->generation++;
    if (changed) m->saved_since_race = 1;
    snprintf(a->status, sizeof a->status, changed ? "saved %s" : "nothing to save: %s is as it was", m->tex[m->sel].file);
    return 1;
}

int app_mask_modified(const App *a)
{
    return a->mask.sel >= 0 && mask_edit_modified((MaskEdit *)&a->mask.me);
}

int app_can_undo(const App *a)
{
    return a->mask.focus && a->mask.sel >= 0 ? mask_edit_can_undo(&a->mask.me) : car_edit_can_undo(&a->edit);
}

int app_can_redo(const App *a)
{
    return a->mask.focus && a->mask.sel >= 0 ? mask_edit_can_redo(&a->mask.me) : car_edit_can_redo(&a->edit);
}

int app_undo(App *a)
{
    return a->mask.focus && a->mask.sel >= 0 ? mask_edit_undo(&a->mask.me) : car_edit_undo(&a->edit);
}

int app_redo(App *a)
{
    return a->mask.focus && a->mask.sel >= 0 ? mask_edit_redo(&a->mask.me) : car_edit_redo(&a->edit);
}
