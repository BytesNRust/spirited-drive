/* The track editor's selected control points as the window uses them (app_select.h). */
#include "app_select.h"
#include "key_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_sel_sync(App *a)
{
    if (a->kind != APP_KIND_TRACK) return;
    if (sel_points(&a->sel) != a->trace.count && !sel_resize(&a->sel, a->trace.count)) return;
    if (a->selected >= a->trace.count) a->selected = a->trace.count - 1;
    if (a->selected < 0) {
        if (sel_count(&a->sel) > 0) sel_clear(&a->sel);
    } else if (!sel_has(&a->sel, a->selected)) {
        sel_click(&a->sel, a->selected, SEL_ONE);
    }
}

/* App.selected after the set changed: `i` when it is in the set, else the one it had when still in, else the first */
static void follow(App *a, int i)
{
    if (sel_has(&a->sel, i)) a->selected = i;
    else if (!sel_has(&a->sel, a->selected)) a->selected = sel_first(&a->sel);
}

void app_sel_click(App *a, int i, SelClick how)
{
    app_sel_sync(a);
    if (i < 0 || i >= a->trace.count) {
        if (how == SEL_ONE) app_sel_clear(a);
        return;
    }
    sel_click(&a->sel, i, how);
    follow(a, i);
}

void app_sel_box(App *a, double x0, double z0, double x1, double z1, int add)
{
    app_sel_sync(a);
    sel_box(&a->sel, a->trace.points, a->trace.count, x0, z0, x1, z1, add);
    follow(a, -1);
}

void app_sel_clear(App *a)
{
    sel_clear(&a->sel);
    a->selected = -1;
}

int app_sel_count(const App *a)
{
    return a->kind == APP_KIND_TRACK ? sel_count(&a->sel) : 0;
}

void app_sel_move(App *a, double dx, double dz)
{
    app_sel_sync(a);
    if (sel_count(&a->sel) == 0) return;
    sel_move(&a->sel, a->trace.points, a->trace.count, dx, dz);
    a->dirty = 1;
}

void app_sel_rotate_live(App *a, double degrees)
{
    app_sel_sync(a);
    if (sel_count(&a->sel) == 0) return;
    if (a->rotate_from == NULL || a->rotate_from_count != a->trace.count) {   /* the field is taken: where the points are now */
        free(a->rotate_from);
        a->rotate_from = malloc(sizeof *a->rotate_from * (size_t)a->trace.count);
        if (a->rotate_from == NULL) return;
        memcpy(a->rotate_from, a->trace.points, sizeof *a->rotate_from * (size_t)a->trace.count);
        a->rotate_from_count = a->trace.count;
    }
    sel_rotate(&a->sel, a->trace.points, a->trace.count, a->rotate_from, degrees);
    a->dirty = 1;
}

void app_sel_rotate_end(App *a)
{
    free(a->rotate_from);
    a->rotate_from = NULL, a->rotate_from_count = 0;
}

void app_sel_rotate_by(App *a, double degrees)
{
    app_sel_rotate_end(a);
    app_sel_rotate_live(a, degrees);
    app_sel_rotate_end(a);
}

/* point i's text in a Road profile row (app_sel_profile) */
static void profile_text(int row, int side, double value, TraceSurface surface, char *out, size_t cap)
{
    const int field = row - 1;
    if (row == 0) snprintf(out, cap, "%s", surface == TRACE_SURFACE_TARMAC ? "Asphalt" : "Off-road");
    else if (field == TRACE_SIDE_WALL) snprintf(out, cap, "%s", value == TRACK_GEN_WALL_SOFT ? "Soft" : "Hard");
    else if (field == TRACE_SIDE_FENCE) snprintf(out, cap, "%s", value != 0 ? "On" : "Off");
    else snprintf(out, cap, "%.2f", value);
    (void)side;
}

int app_sel_profile(const App *a, int row, int side, char *out, size_t cap)
{
    const int count = a->trace.count;
    if (cap == 0) return -1;
    out[0] = '\0';
    if (sel_count(&a->sel) == 0 || row < 0 || row >= APP_SEL_PROFILE_ROWS || (side != TRACK_GEN_LEFT && side != TRACK_GEN_RIGHT)) return -1;
    double (*sides)[TRACE_SIDE_FIELDS][2] = malloc(sizeof *sides * (size_t)count);
    TraceSurface *left = malloc(sizeof *left * (size_t)count), *right = malloc(sizeof *right * (size_t)count);
    int mixed = -1;
    if (sides != NULL && left != NULL && right != NULL) {
        track_trace_point_sides(a->trace.points, count, a->trace.keys, a->trace.key_count, sides);
        track_trace_point_surfaces(a->trace.keys, a->trace.key_count, count, left, right);
        mixed = 0;
        char text[32];
        for (int i = 0; i < count && i < a->sel.points && !mixed; ++i) {
            if (!sel_has(&a->sel, i)) continue;
            profile_text(row, side, row > 0 ? sides[i][row - 1][side] : 0, side == TRACK_GEN_LEFT ? left[i] : right[i], text, sizeof text);
            if (out[0] == '\0') snprintf(out, cap, "%s", text);
            else if (strcmp(out, text) != 0) mixed = 1;
        }
        if (mixed) snprintf(out, cap, "%s", APP_SEL_MIXED);
    }
    free(sides), free(left), free(right);
    return mixed;
}

void app_remove_selection(App *a)
{
    if (a->kind != APP_KIND_TRACK) return;
    app_sel_sync(a);
    if (!sel_can_remove(&a->sel, a->status, sizeof a->status)) return;
    const int removed = sel_count(&a->sel), first = sel_first(&a->sel);
    for (int i = a->trace.count - 1; i >= 0; --i) {   /* from the last, so the indices before it hold */
        if (!sel_has(&a->sel, i)) continue;
        memmove(a->trace.points + i, a->trace.points + i + 1, sizeof *a->trace.points * (size_t)(a->trace.count - i - 1));
        a->trace.count--;
        track_trace_keys_remove_point(a->trace.keys, &a->trace.key_count, i);   /* its key goes with it */
        sel_removed(&a->sel, i);
    }
    a->selected = first < a->trace.count ? first : a->trace.count - 1;
    sel_click(&a->sel, a->selected, SEL_ONE);
    a->dirty = 1;
    if (removed == 1) snprintf(a->status, sizeof a->status, "point removed");
    else snprintf(a->status, sizeof a->status, "%d points removed", removed);
}

unsigned app_sel_mixed(const App *a)
{
    if (a->kind != APP_KIND_TRACK || sel_count(&a->sel) < 2) return 0;
    KeyState st;
    if (!key_state_make(&a->trace, &st)) return 0;
    const int first = sel_first(&a->sel);
    const TraceKey base = key_state_at(&st, first, KEY_RAMPED | KEY_HELD | TRACE_KEY_WIDTH);
    unsigned mixed = 0;
    for (int i = first + 1; i < a->trace.count; ++i) {
        if (!sel_has(&a->sel, i)) continue;
        const TraceKey k = key_state_at(&st, i, KEY_RAMPED | KEY_HELD | TRACE_KEY_WIDTH);
        for (unsigned bit = 1; bit != 0 && bit <= (KEY_RAMPED | KEY_HELD); bit <<= 1)
            if ((bit & (KEY_RAMPED | KEY_HELD)) && !key_same(&base, &k, bit)) mixed |= bit;
        for (int side = 0; side < 2; ++side)
            if (st.surface[side][i] != st.surface[side][first]) mixed |= APP_SEL_MIXED_SURFACE(side);
    }
    key_state_free(&st);
    return mixed;
}
