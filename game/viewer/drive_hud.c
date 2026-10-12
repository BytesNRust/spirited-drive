#include "viewer/drive_hud.h"

#include "game/platform/platform.h"
#include "game/gfx/blit.h"
#include "game/gfx/flat.h"
#include "game/sys/palette.h"
#include "game/race/dash_load.h"
#include "game/race/hud_keys.h"
#include "game/race/race_chase.h"
#include "game/race/hud_mask.h"
#include "game/race/hud_redraw.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum {
    HIGH_RES = 1, ENGLISH = 0, SHAPE_8_BIT = 0x7b,
    LIVE_RACE = 1,      /* 004c5390: 00455a5c sets 1 once the race runs (0044e700 2 / 3 in replays) */
    SHADE_SCALE = 0x9999 /* 00446bac's 00482698 */
};

/* A name 00450838 builds in mixed case; the CD's files are upper case. */
static uint8_t *read_named(const char *path, size_t dir_length)
{
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file != NULL) return file;
    char upper[DASH_PATH];
    snprintf(upper, sizeof upper, "%s", path);
    for (size_t i = dir_length; upper[i] != '\0'; i++) upper[i] = (char)toupper((unsigned char)upper[i]);
    return platform_read_file(upper, &size);
}

static uint8_t *read_sized(const char *path, size_t dir_length, size_t *size)
{
    uint8_t *file = platform_read_file(path, size);
    if (file != NULL) return file;
    char upper[COCKPIT_PATH];
    snprintf(upper, sizeof upper, "%s", path);
    for (size_t i = dir_length; upper[i] != '\0'; i++) upper[i] = (char)toupper((unsigned char)upper[i]);
    return platform_read_file(upper, size);
}

int drive_hud_cockpit(DriveHud *h, RaceState *race, const char *data_root, const char *stem)
{
    char dir[COCKPIT_PATH];
    const int n = snprintf(dir, sizeof dir, "%s/SIMDATA/DASH/", data_root);
    CockpitFiles files;
    if (n < 0 || n >= (int)sizeof dir || cockpit_paths(&files, dir, stem, HIGH_RES) != 0) return 0;
    size_t bank_size = 0, fmm_size = 0;
    h->dash_bank = read_sized(files.bank, (size_t)n, &bank_size);
    h->dash_fmm = read_sized(files.fmm, (size_t)n, &fmm_size);
    h->cockpit_ready = h->dash_bank != NULL && h->dash_fmm != NULL && fmm_size >= 0x18 &&
                       cockpit_load(&h->cockpit, &h->cockpit_world, &h->hud, race, &h->keys.dash_camera, h->dash_bank,
                                    bank_size, h->dash_fmm, stem, HIGH_RES, 1) == 0;
    if (!h->cockpit_ready) fprintf(stderr, "drive_hud: no dashboard from %s\n", files.bank);
    return h->cockpit_ready;
}

int drive_hud_read(DriveHud *h, const char *data_root)
{
    memset(h, 0, sizeof *h);
    char dir[DASH_PATH];
    const int n = snprintf(dir, sizeof dir, "%s/SIMDATA/MISC/", data_root);
    DashFiles files;
    if (n < 0 || n >= (int)sizeof dir || dash_load_paths(&files, dir, ENGLISH, HIGH_RES) != 0) return 0;
    h->bank = read_named(files.bank, (size_t)n);
    h->mmb = read_named(files.mmb, (size_t)n);
    if (h->bank == NULL || h->mmb == NULL) {
        fprintf(stderr, "drive_hud: no %s or %s\n", files.bank, files.mmb);
        drive_hud_free(h);
        return 0;
    }
    return 1;
}

void drive_hud_setup(DriveHud *h, RacePalette *p)
{
    if (h->bank == NULL) return;
    /* TODO(00483e60): the view-mask parts (.b%d%d) are not read: art.parts stays NULL. */
    h->ready = dash_load_setup(&h->art, &h->replay, p, h->bank, h->mmb, HIGH_RES, 0) == 0;
    if (!h->ready) fprintf(stderr, "drive_hud: the mask bank lacks a shape the HUD needs\n");
}


void drive_hud_start(DriveHud *h, RacePalette *p)
{
    if (!h->ready) return;
    /* TODO(00447098): the queued banks' remap; here the mask bank alone. */
    race_palette_remap_track_bank(p, h->bank, h->art.palette);
    h->strip_colour = (uint32_t)race_palette_nearest(p, 0, 0, 0);
    h->needle_colour = (uint32_t)race_palette_nearest(p, 0x3f, 0, 0);  /* 00446bac */
    uint8_t copy[RACE_PALETTE_COLOURS * 3];  /* 0050ad34; the search's entries 004c5ff8 as 0041f818 left them */
    race_palette_read(p, 0, RACE_PALETTE_COLOURS, copy);
    palette_shade_table(copy, p->enabled, SHADE_SCALE, h->shade);
    memset(h->back, 0, sizeof h->back);
    h->back[0] = SHAPE_8_BIT;
    h->back[4] = DRIVE_BACK_W, h->back[6] = DRIVE_BACK_H;
    h->in_car = 0;
    memset(&h->needle, 0, sizeof h->needle);
    memset(&h->hud, 0, sizeof h->hud);
    hud_reset(&h->hud);
    h->hud.mode = 0;
    h->hud.redraw = 1;
    memset(h->strip, (int)h->strip_colour, sizeof h->strip);
    memset(h->view, 0, DRIVE_VIEW_HEADER);
    h->view[0] = SHAPE_8_BIT;
    h->view[4] = DRIVE_VIEW_W & 0xff, h->view[5] = DRIVE_VIEW_W >> 8;
    h->view[6] = DRIVE_VIEW_H & 0xff, h->view[7] = DRIVE_VIEW_H >> 8;
    memset(h->view + DRIVE_VIEW_HEADER, (int)h->strip_colour, sizeof h->view - DRIVE_VIEW_HEADER);
    h->changed = 0;
    h->shown_view = -1;
    memset(&h->keys, 0, sizeof h->keys);
    h->keys.armed = 1;  /* 004b9460's start */
}

uint8_t *drive_hud_view(DriveHud *h, RaceState *race, uint8_t *pixels)
{
    if (!h->ready) return pixels;
    const RaceCamera *camera = race->chase.camera;
    const int32_t view = camera != NULL ? camera->view : 0;
    if (view != h->shown_view) h->shown_view = view, h->changed = 1;
    if (h->changed) {
        const QuadTarget t = { h->view + DRIVE_VIEW_HEADER, DRIVE_VIEW_W, 0, 0, DRIVE_VIEW_W, DRIVE_VIEW_H };
        const ViewRow *row = camera != NULL ? camera->view_row : NULL;
        const HudRedrawInputs in = {
            row != NULL ? (HudFrame){ row->unknown_14[4], row->unknown_14[5], row->unknown_14[6], row->unknown_14[7] }
                        : (HudFrame){ 0, 0, 0, 0 },
            1, DRIVE_VIEW_W, DRIVE_VIEW_H, race->splits.gap, race->splits.gap_shown, HIGH_RES, h->strip_colour };
        hud_redraw(&t, &h->hud, &h->art, race, &in);
    }
    return h->view + DRIVE_VIEW_HEADER;
}

/* 00421c18's surfaces: the view surface and the frame (004c2da0 and the
 * default are both the frame here). */
typedef struct {
    DriveHud *h;
    QuadTarget screen, view, *target;
} MaskSurfaces;

static void mask_select(void *ctx, HudSurface s)
{
    MaskSurfaces *m = ctx;
    m->target = s == HUD_SURFACE_VIEW ? &m->view : &m->screen;
}

static void mask_clip(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom)
{
    MaskSurfaces *m = ctx;
    m->target->clip_left = left, m->target->clip_right = right;
    m->target->clip_top = top, m->target->clip_bottom = bottom;
}

static void mask_fill(void *ctx, uint32_t colour)
{
    const QuadTarget *t = ((MaskSurfaces *)ctx)->target;
    flat_fill_rect(t, t->clip_left, t->clip_top, t->clip_right - t->clip_left, t->clip_bottom - t->clip_top, colour);
}

static void mask_draw(void *ctx, uint8_t *shape)
{
    blit_shape_placed_transparent(((MaskSurfaces *)ctx)->target, shape);
}

static HudViewClipInputs view_clip_inputs(int width, int height)
{
    return (HudViewClipInputs){ LIVE_RACE, HIGH_RES, width, height, width, height };
}

static void mask_view_clip(void *ctx)
{
    MaskSurfaces *m = ctx;
    const HudViewClipInputs in = view_clip_inputs(DRIVE_VIEW_W, DRIVE_VIEW_H);
    hud_view_clip(m->target, &m->h->hud, &in);
}

static void mask_present(void *ctx)
{
    MaskSurfaces *m = ctx;
    blit_shape(m->target, m->h->view, 0, 0, BLIT_OPAQUE);
}

static void mask_runs(void *ctx, const uint8_t *runs)
{
    MaskSurfaces *m = ctx;
    blit_runs(m->target, DRIVE_VIEW_W, m->h->view, runs);
}

static void draw_mask(DriveHud *h, const RaceState *race, uint8_t *pixels, int width, int height)
{
    MaskSurfaces m = { h, { pixels, width, 0, 0, width, height },
                       { h->view + DRIVE_VIEW_HEADER, DRIVE_VIEW_W, 0, 0, DRIVE_VIEW_W, DRIVE_VIEW_H }, NULL };
    m.target = &m.screen;
    const HudMaskOps ops = { &m, mask_select, mask_clip, mask_fill, mask_draw, mask_view_clip, mask_present, mask_runs };
    const RaceCamera *camera = race->chase.camera;
    const HudMaskInputs in = { camera != NULL ? camera->view : 0, 0, 0, 0, 0, width, height, h->strip_colour };
    uint8_t clear_view = 0;
    hud_mask_draw(&ops, &h->art, &h->hud, &clear_view, &in, h->changed);
    h->changed = 0;
}

/* 00420710's calls: the labels into the strip, the chase's kind taken
 * again; 00420918 is not reached (the parts key is unmapped). */
typedef struct {
    DriveHud *h;
    RaceState *race;
    const QuadTarget *strip;
} KeysCalls;

static void keys_labels(void *ctx)
{
    KeysCalls *k = ctx;
    hud_draw_labels(k->strip, &k->h->hud, &k->h->art, k->race, k->race->splits.gap, k->race->splits.gap_shown, HIGH_RES,
                    k->h->strip_colour);
}

static void keys_parts_redraw(void *ctx)
{
    (void)ctx;  /* TODO(00420918): with the parts key, once the .b%d%d parts are read */
}

static void keys_take(void *ctx, int32_t kind)
{
    KeysCalls *k = ctx;
    race_chase_take(k->race, &k->race->chase, kind);
}

static void draw_keys(DriveHud *h, RaceState *race, const QuadTarget *strip)
{
    uint8_t keys[0x100] = { 0 };
    for (int code = HUD_KEY_REDRAW; code <= HUD_KEY_CAMERA; code++) keys[code] = platform_key_down(code) ? 1 : 0;
    KeysCalls calls = { h, race, strip };
    const HudKeysOps ops = { &calls, keys_labels, keys_parts_redraw, keys_take };
    /* parts_key 0: no key has set-1 code 0, so the parts mode stays off; no pad */
    const HudKeysInputs in = { keys, 0, 0, { 0, 0 }, 0, 0 };
    hud_keys(&ops, &h->hud, &h->keys, race, &in);
    if (h->keys.view_changed == 999) h->changed = 1;  /* 00511fec: the full redraw and 00421c18(1) */
    h->keys.view_changed = 0;
}

/* 00421160's surfaces: the frame (004c2da0 and the default both) and 004c2d98. */
typedef struct {
    DriveHud *h;
    RaceState *race;
    QuadTarget screen, back, target;
} CockpitSurfaces;

static QuadTarget *cockpit_select(void *ctx, HudSurface s)
{
    CockpitSurfaces *c = ctx;
    c->target = s == HUD_SURFACE_BACK ? c->back : c->screen;
    return &c->target;
}

static void cockpit_labels(void *ctx)
{
    CockpitSurfaces *c = ctx;
    const QuadTarget strip = { c->h->strip, DRIVE_HUD_W, 0, 0, DRIVE_HUD_W, DRIVE_HUD_H };
    hud_draw_labels(&strip, &c->h->hud, &c->h->art, c->race, c->race->splits.gap, c->race->splits.gap_shown,
                    HIGH_RES, c->h->strip_colour);
}

/* 0042270c's part that is ported: 00421160 while 004f9a24 / 004f9a28 is set.
 * TODO(0042270c): the rest (the view through 00483fa0's runs, the wheel, the
 * gauges); until then the view goes to the frame whole and the dash over it. */
static void draw_cockpit(DriveHud *h, RaceState *race, uint8_t *pixels, int width, int height)
{
    if (!h->in_car) h->hud.unknown_4f9a20[1] = 1, h->in_car = 1;  /* 0041e364 */
    CockpitSurfaces s = { h, race, { pixels, width, 0, 0, width, height },
                          { h->back + DRIVE_VIEW_HEADER, DRIVE_BACK_W, 0, 0, DRIVE_BACK_W, DRIVE_BACK_H }, { 0 } };
    blit_shape(&s.screen, h->view, 0, 0, BLIT_OPAQUE);
    if (h->hud.unknown_4f9a20[1] == 0 && h->hud.unknown_4f9a20[2] == 0) {
        blit_shape_at_transparent(&s.screen, h->cockpit.dash, 0, 0);
        return;
    }
    const CockpitRedrawOps ops = { &s, cockpit_select, cockpit_labels };
    const CockpitRedrawInputs in = { 0, view_clip_inputs(width, height), h->back, h->shade };
    cockpit_redraw(&ops, &h->hud, &h->cockpit, &h->cockpit_world, &in);
    h->hud.unknown_4f9a20[1] = h->hud.unknown_4f9a20[2] = 0;
}

void drive_hud_draw(DriveHud *h, RaceState *race, int32_t redline, uint8_t *pixels, int width, int height)
{
    if (!h->ready) return;
    const Car *car = race->cars[race_slot(race->viewed)];
    if (car == NULL) return;
    const int full = pixels != NULL && width == DRIVE_VIEW_W && height == DRIVE_VIEW_H;
    const int in_car = h->cockpit_ready && race->chase.kind == 0 && h->keys.dash_camera == 0;  /* 0044cc74 */
    if (!in_car) h->in_car = 0;
    if (full && in_car) draw_cockpit(h, race, pixels, width, height);
    else if (full) draw_mask(h, race, pixels, width, height);
    const QuadTarget t = { h->strip, DRIVE_HUD_W, 0, 0, DRIVE_HUD_W, DRIVE_HUD_H };
    draw_keys(h, race, &t);
    hud_draw_top_row(&t, &h->hud, &h->art, race, car, ENGLISH, HIGH_RES, h->strip_colour);
    if (pixels == NULL) return;
    const int w = width < DRIVE_HUD_W ? width : DRIVE_HUD_W;
    for (int y = 0; y < DRIVE_HUD_H && y < height; y++)
        memcpy(pixels + (size_t)y * (size_t)width, h->strip + (size_t)y * DRIVE_HUD_W, (size_t)w);
    if (in_car) return;  /* 0042270c does not run 0042221c */
    QuadTarget screen = { pixels, width, 0, 0, width, height };
    hud_draw_needle(&screen, &h->needle, &h->art, car->rpm, redline, width, height, width, height, HIGH_RES,
                    h->needle_colour);
}

void drive_hud_free(DriveHud *h)
{
    platform_free_file(h->bank);
    platform_free_file(h->mmb);
    platform_free_file(h->dash_bank);
    platform_free_file(h->dash_fmm);
    h->bank = h->mmb = h->dash_bank = h->dash_fmm = NULL;
    h->cockpit_ready = 0;
    h->ready = 0;
}
