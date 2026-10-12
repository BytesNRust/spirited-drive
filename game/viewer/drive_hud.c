#include "viewer/drive_hud.h"

#include "game/platform/platform.h"
#include "game/gfx/blit.h"
#include "game/gfx/flat.h"
#include "game/sys/palette.h"
#include "game/sys/resource.h"
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
    snprintf(h->dash_path, sizeof h->dash_path, "%s", files.bank);
    snprintf(h->dash_stem, sizeof h->dash_stem, "%s", stem);
    snprintf(h->misc_dir, sizeof h->misc_dir, "%s/SIMDATA/MISC/", data_root);
    h->dash_dir_length = (size_t)n;
    h->dash_bank = read_sized(files.bank, (size_t)n, &bank_size);
    h->dash_fmm = read_sized(files.fmm, (size_t)n, &fmm_size);
    h->cockpit_ready = h->dash_bank != NULL && h->dash_fmm != NULL && fmm_size >= 0x18 &&
                       cockpit_load(&h->cockpit, &h->cockpit_world, &h->hud, race, &h->keys.dash_camera, h->dash_bank,
                                    bank_size, h->dash_fmm, stem, HIGH_RES, 1) == 0;
    if (!h->cockpit_ready) fprintf(stderr, "drive_hud: no dashboard from %s\n", files.bank);
    return h->cockpit_ready;
}

static void free_parts(DriveHud *h)
{
    for (int r = 0; r < DASH_PART_ROWS; r++)
        for (int c = 0; c < DASH_PART_COLUMNS; c++) platform_free_file(h->parts[r][c]), h->parts[r][c] = NULL;
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
    int parts = 1;
    for (int r = 0; r < DASH_PART_ROWS; r++)
        for (int c = 0; c < DASH_PART_COLUMNS; c++)
            parts = parts && (h->parts[r][c] = read_named(files.parts[r][c], (size_t)n)) != NULL;
    if (!parts) {
        fprintf(stderr, "drive_hud: no %s and the other parts, the mask from %s\n", files.parts[0][0], files.mmb);
        free_parts(h);
    }
    return 1;
}

void drive_hud_setup(DriveHud *h, RacePalette *p)
{
    if (h->bank == NULL) return;
    h->ready = dash_load_setup(&h->art, &h->replay, p, h->bank, h->mmb, HIGH_RES, 0) == 0;
    memcpy(h->art.parts, h->parts, sizeof h->parts);
    if (!h->ready) fprintf(stderr, "drive_hud: the mask bank lacks a shape the HUD needs\n");
}


void drive_hud_start(DriveHud *h, RacePalette *p)
{
    if (!h->ready) return;
    /* TODO(00447098): the queued banks' remap; here the mask bank alone. */
    race_palette_remap_track_bank(p, h->bank, h->art.palette);
    h->strip_colour = (uint32_t)race_palette_nearest(p, 0, 0, 0);
    h->needle_colour = (uint32_t)race_palette_nearest(p, 0x3f, 0, 0);  /* 00446bac */
    h->road_colour = (uint32_t)race_palette_nearest(p, 0x3f, 0x3f, 0x3f);
    h->frame_colour = (uint32_t)race_palette_nearest(p, 0x1a, 0x1a, 0x1a);
    const uint8_t car_colours[HUD_MAP_COLOURS] = { 0xf5, 0xf8, 0xf0, 0xfa, (uint8_t)h->road_colour, 0xf9, 0xfc, 0xfb };
    memcpy(h->car_colours, car_colours, sizeof car_colours);
    uint8_t copy[RACE_PALETTE_COLOURS * 3];  /* 0050ad34; the search's entries 004c5ff8 as 0041f818 left them */
    race_palette_read(p, 0, RACE_PALETTE_COLOURS, copy);
    palette_shade_table(copy, p->enabled, SHADE_SCALE, h->shade);
    h->palette = p;
    memset(&h->police, 0, sizeof h->police);
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
    h->keys.use_parts = 1;  /* 0044ca3c */
    memset(h->map, 0, sizeof h->map);
    h->map[0] = SHAPE_8_BIT;
    h->map[4] = DRIVE_MAP_W, h->map[6] = DRIVE_MAP_H;
    h->map_blink = 0;
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
    const int32_t use_parts = h->keys.use_parts != 0 && h->parts[0][0] != NULL;  /* 004f9b3c, while the parts were read */
    const HudMaskInputs in = { camera != NULL ? camera->view : 0, 0, 0, use_parts, 0, width, height, h->strip_colour };
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

static void cockpit_redraw_now(DriveHud *h, CockpitSurfaces *s)
{
    const CockpitRedrawOps ops = { s, cockpit_select, cockpit_labels };
    const CockpitRedrawInputs in = { 0, view_clip_inputs(s->screen.pitch, s->screen.clip_bottom), h->back, h->shade };
    cockpit_redraw(&ops, &h->hud, &h->cockpit, &h->cockpit_world, &in);
}

/* The frame's colours from the race palette's after 00476460 wrote it. */
static void rgb_from_palette(DriveHud *h)
{
    if (h->rgb == NULL || h->palette == NULL) return;
    for (int i = 0; i < RACE_PALETTE_COLOURS; i++)
        for (int k = 0; k < 3; k++) h->rgb[3 * i + k] = h->palette->screen_rgbx[4 * i + k];
}

/* 004214e4's calls: the file from SIMDATA/MISC (upper case on a case-sensitive file system; the original's dir
 * 005246e8 is passed empty, the port's path being longer than its 80 bytes), the race palette. */
static void police_free_bank(void *ctx, uint8_t *bank)
{
    DriveHud *h = ((CockpitSurfaces *)ctx)->h;
    (void)bank;
    platform_free_file(h->dash_bank);
    h->dash_bank = NULL;
}

static uint8_t *police_load(void *ctx, const char *name)
{
    const DriveHud *h = ((CockpitSurfaces *)ctx)->h;
    char path[2 * COCKPIT_PATH];
    const size_t dir = strlen(h->misc_dir);
    snprintf(path, sizeof path, "%s%s", h->misc_dir, name);
    size_t size = 0;
    uint8_t *file = resource_load(path, &size);
    if (file != NULL) return file;
    for (size_t i = dir; path[i] != '\0'; i++) path[i] = (char)toupper((unsigned char)path[i]);
    return resource_load(path, &size);
}

static void police_palette_read(void *ctx)
{
    DriveHud *h = ((CockpitSurfaces *)ctx)->h;
    race_palette_read(h->palette, 0, RACE_PALETTE_COLOURS, h->colours.saved);
}

static void police_palette_write(void *ctx, const uint8_t *colours)
{
    DriveHud *h = ((CockpitSurfaces *)ctx)->h;
    race_palette_write(h->palette, 0, 0xfe, colours);
    rgb_from_palette(h);
}

static void police_palette_bank(void *ctx, uint8_t *bank, const uint8_t *palette)
{
    race_palette_remap_track_bank(((CockpitSurfaces *)ctx)->h->palette, bank, palette);
}

static void police_movie(void *ctx) { (void)ctx; }  /* 00464240: the word 00524e63 is 0 in --drive */

static void draw_police(DriveHud *h, RaceState *race, CockpitSurfaces *s)
{
    const CockpitPoliceOps ops = { s, police_free_bank, police_load, police_palette_read, police_palette_write,
                                   police_palette_bank, cockpit_select, police_movie };
    const CockpitPoliceInputs in = { 0, race->stop_notice[0], 0, 0, "",
                                     view_clip_inputs(s->screen.pitch, s->screen.clip_bottom) };
    const int had = h->police.screen_bank != NULL;
    if (cockpit_police(&ops, &h->cockpit, &h->police, &h->hud, &in) != 0 && !had)
        fprintf(stderr, "drive_hud: no police screen\n");
}

/* 004217a8's calls. */
static void reload_free_saved(void *ctx)
{
    DriveHud *h = ((CockpitSurfaces *)ctx)->h;
    resource_free(h->police.screen_bank);
    h->police.screen_bank = NULL, h->police.screen = NULL;
}

static void reload_none(void *ctx) { (void)ctx; }  /* 00464a30 (no back buffer here); TODO(00473614) */

static void reload_load(void *ctx)
{
    CockpitSurfaces *s = ctx;
    DriveHud *h = s->h;
    size_t size = 0;
    h->dash_bank = read_sized(h->dash_path, h->dash_dir_length, &size);
    if (h->dash_bank == NULL || cockpit_load(&h->cockpit, &h->cockpit_world, &h->hud, s->race, &h->keys.dash_camera,
                                             h->dash_bank, size, h->dash_fmm, h->dash_stem, HIGH_RES, 0) != 0) {
        fprintf(stderr, "drive_hud: the dashboard did not load again\n");
        h->cockpit_ready = 0;
    }
}

static void reload_palette_copy(void *ctx)
{
    DriveHud *h = ((CockpitSurfaces *)ctx)->h;
    race_palette_write(h->palette, 0, 0xfe, h->colours.saved);
    rgb_from_palette(h);
}

static void reload_palette_bank(void *ctx, uint8_t *bank, uint8_t *palette)
{
    if (bank != NULL && palette != NULL) race_palette_remap_track_bank(((CockpitSurfaces *)ctx)->h->palette, bank, palette);
}

static void reload_redraw(void *ctx)
{
    CockpitSurfaces *s = ctx;
    if (s->h->cockpit_ready) cockpit_redraw_now(s->h, s);
}

static void reload_cockpit(DriveHud *h, RaceState *race, CockpitSurfaces *s)
{
    const CockpitReloadOps ops = { s, reload_free_saved, reload_none, cockpit_select, reload_none, reload_load,
                                   reload_palette_copy, reload_palette_bank, reload_redraw };
    CockpitReloadState st = { h->police.screen_bank != NULL, 0, 0 };
    const CockpitReloadInputs in = { 0, 0, race->stop_notice[0], race->stop_notice[1], 0, h->keys.dash_camera,
                                     race->chase.kind, LIVE_RACE };
    cockpit_reload(&ops, &h->cockpit, &st, &in);
}

/* 0042270c's part that is ported: 00421160 while 004f9a24 / 004f9a28 is set.
 * TODO(0042270c): the rest (the view through 00483fa0's runs, the wheel, the
 * gauges); until then the view goes to the frame whole and the dash over it. */
static void draw_cockpit(DriveHud *h, CockpitSurfaces *s)
{
    if (!h->in_car) h->hud.unknown_4f9a20[1] = 1, h->in_car = 1;  /* 0041e364 */
    blit_shape(&s->screen, h->view, 0, 0, BLIT_OPAQUE);
    if (h->hud.unknown_4f9a20[1] == 0 && h->hud.unknown_4f9a20[2] == 0) {
        blit_shape_at_transparent(&s->screen, h->cockpit.dash, 0, 0);
        return;
    }
    cockpit_redraw_now(h, s);
    h->hud.unknown_4f9a20[1] = h->hud.unknown_4f9a20[2] = 0;
}

static QuadTarget *map_select(void *ctx, HudSurface s)
{
    (void)s;  /* 00422ce0 selects only 004c2d9c */
    return ctx;
}

/* 00422ce0 into the map's surface, from the race's and the cockpit's words. */
static void draw_map(DriveHud *h, const RaceState *race)
{
    QuadTarget t = { h->map + DRIVE_VIEW_HEADER, DRIVE_MAP_W, 0, 0, DRIVE_MAP_W, DRIVE_MAP_H };
    const HudMapOps ops = { &t, map_select };
    const Car *police = race->cars[2];
    const int32_t ring = race->circuit ? race->segments * 4 : 0x20000;  /* 0043e4e0 */
    HudMapInputs in = {
        race->tri, race->circuit ? ring - 1 : -1, ring, h->cockpit_world.unknown_4f9b40, HIGH_RES,
        h->keys.use_parts, race->mirrored, race->race_type, race->track, race->track_variant,
        race->chase.kind, h->keys.dash_camera, h->cockpit_world.back_saved, h->back,
        h->bank, race->viewed, { 0 }, police != NULL, police != NULL ? police->flags_51a : 0,
        race->unknown_4fbab8, h->hud.place, race->car_places, h->car_colours, h->frame_colour,
        h->road_colour, h->needle_colour, DRIVE_MAP_W, DRIVE_MAP_H };
    for (int i = 0; i < HUD_MAP_CARS; i++)
        in.car_node[i] = race->cars[i] != NULL ? (int16_t)race->cars[i]->node : 0;  /* port guard: no car, node 0 */
    hud_map_draw(&ops, &in, &h->map_blink);
}

/* 00422598 / 0042270c's start: while a stop notice is up, 004214e4 and nothing else this frame (then 004b9568, the
 * full redraw, or 004f9a24); else 004217a8, the dash loaded again when the notice freed it.
 * TODO(00405260, 0044d640): 00422598's other calls with the notice. */
static int police_or_reload(DriveHud *h, RaceState *race, CockpitSurfaces *s, int in_car)
{
    if (race->stop_notice[0] != 0 || race->stop_notice[1] != 0) {
        draw_police(h, race, s);
        if (in_car) h->hud.unknown_4f9a20[1] = 1;
        else h->changed = 1;
        return 1;
    }
    if (h->dash_fmm != NULL && h->cockpit.bank == NULL) reload_cockpit(h, race, s);
    return 0;
}

void drive_hud_draw(DriveHud *h, RaceState *race, int32_t redline, uint8_t *pixels, int width, int height)
{
    if (!h->ready) return;
    const Car *car = race->cars[race_slot(race->viewed)];
    if (car == NULL) return;
    const int full = pixels != NULL && width == DRIVE_VIEW_W && height == DRIVE_VIEW_H;
    int in_car = h->cockpit_ready && race->chase.kind == 0 && h->keys.dash_camera == 0;  /* 0044cc74 */
    if (!in_car) h->in_car = 0;
    if (full) {
        if (h->cockpit_ready) draw_map(h, race);
        CockpitSurfaces s = { h, race, { pixels, width, 0, 0, width, height },
                              { h->back + DRIVE_VIEW_HEADER, DRIVE_BACK_W, 0, 0, DRIVE_BACK_W, DRIVE_BACK_H }, { 0 } };
        if (police_or_reload(h, race, &s, in_car)) return;
        in_car = in_car && h->cockpit_ready;
        if (in_car) draw_cockpit(h, &s);
        else draw_mask(h, race, pixels, width, height);
    }
    const QuadTarget t = { h->strip, DRIVE_HUD_W, 0, 0, DRIVE_HUD_W, DRIVE_HUD_H };
    draw_keys(h, race, &t);
    hud_draw_top_row(&t, &h->hud, &h->art, race, car, ENGLISH, HIGH_RES, h->strip_colour);
    if (pixels == NULL) return;
    const int w = width < DRIVE_HUD_W ? width : DRIVE_HUD_W;
    for (int y = 0; y < DRIVE_HUD_H && y < height; y++)
        memcpy(pixels + (size_t)y * (size_t)width, h->strip + (size_t)y * DRIVE_HUD_W, (size_t)w);
    QuadTarget screen = { pixels, width, 0, 0, width, height };
    if (!in_car)  /* 0042270c does not run 0042221c */
        hud_draw_needle(&screen, &h->needle, &h->art, car->rpm, redline, width, height, width, height, HIGH_RES,
                        h->needle_colour);
    if (full && h->cockpit_ready && h->keys.use_parts != 0)
        blit_shape_at(&screen, h->map, DRIVE_MAP_X, DRIVE_MAP_Y);
}

void drive_hud_free(DriveHud *h)
{
    platform_free_file(h->bank);
    platform_free_file(h->mmb);
    platform_free_file(h->dash_bank);
    platform_free_file(h->dash_fmm);
    free_parts(h);
    resource_free(h->police.screen_bank);
    h->police.screen_bank = NULL, h->police.screen = NULL;
    h->bank = h->mmb = h->dash_bank = h->dash_fmm = NULL;
    h->cockpit_ready = 0;
    h->ready = 0;
}
