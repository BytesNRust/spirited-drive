#include "viewer/drive_hud.h"
#include "viewer/drive_hud_mask.h"
#include "viewer/dash_box.h"

#include "game/platform/platform.h"
#include "game/gfx/blit.h"
#include "game/gfx/clip.h"
#include "game/gfx/flat.h"
#include "game/sys/key_codes.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/race/dash_controls.h"
#include "game/race/dash_load.h"
#include "game/race/hud_keys.h"
#include "game/race/race_chase.h"
#include "game/race/race_panel.h"
#include "game/race/race_view.h"
#include "game/race/hud_colours.h"
#include "game/race/hud_frame.h"
#include "game/race/hud_redraw.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum {
    HIGH_RES = DRIVE_HUD_HIGH_RES, SHAPE_8_BIT = 0x7b,
    LIVE_RACE = DRIVE_HUD_LIVE_RACE,
    PARTS_KEY_CONFIG = 0x4d /* 0052ad3e's default, 'M' */
};

enum { ROOTED_PATH = 1024 };

/* A name 00450838 / 0041e3f4 builds in its 80 bytes as the original's, under the data root ("SIMDATA/..."): read
 * from the root, upper case past the directory when the mixed-case name is missing (the CD's files are). The root
 * stays out of those 80 bytes, which a long one would cut. */
static uint8_t *read_sized(const char *root, const char *path, size_t dir_length, size_t *size)
{
    char full[ROOTED_PATH];
    const int n = snprintf(full, sizeof full, "%s/%s", root, path);
    if (n < 0 || n >= (int)sizeof full) return NULL;
    uint8_t *file = platform_read_file(full, size);
    if (file != NULL) return file;
    for (size_t i = strlen(root) + 1 + dir_length; full[i] != '\0'; i++) full[i] = (char)toupper((unsigned char)full[i]);
    return platform_read_file(full, size);
}

static uint8_t *read_named(const char *root, const char *path, size_t dir_length)
{
    size_t size = 0;
    return read_sized(root, path, dir_length, &size);
}

int drive_hud_cockpit(DriveHud *h, RaceState *race, const char *data_root, const char *stem)
{
    char dir[COCKPIT_PATH];
    /* port: the exe's German cockpits are SIMDATA/GDASH/ (0042a188, 0041f9a0): its speedo reads km/h, and in the frame
     * it differs from DASH's mph dial only in those numbers. So the dial follows the units, as the needle does. */
    const int n = snprintf(dir, sizeof dir, "%s", resource_kmh() ? "SIMDATA/GDASH/" : "SIMDATA/DASH/");
    snprintf(h->root, sizeof h->root, "%s", data_root);
    CockpitFiles files;
    if (n < 0 || n >= (int)sizeof dir || cockpit_paths(&files, dir, stem, HIGH_RES) != 0) return 0;
    size_t bank_size = 0, fmm_size = 0;
    snprintf(h->dash_path, sizeof h->dash_path, "%s", files.bank);
    snprintf(h->dash_stem, sizeof h->dash_stem, "%s", stem);
    snprintf(h->misc_dir, sizeof h->misc_dir, "SIMDATA/MISC/");
    h->dash_dir_length = (size_t)n;
    h->dash_bank = read_sized(h->root, files.bank, (size_t)n, &bank_size);
    h->dash_fmm = read_sized(h->root, files.fmm, (size_t)n, &fmm_size);
    h->cockpit_ready = h->dash_bank != NULL && h->dash_fmm != NULL && fmm_size >= 0x18 &&
                       cockpit_load(&h->cockpit, &h->cockpit_world, &h->hud, race, &h->keys.dash_camera, h->dash_bank,
                                    bank_size, h->dash_fmm, stem, HIGH_RES, 1) == 0;
    if (!h->cockpit_ready) port_log("drive_hud: no dashboard from %s\n", files.bank);
    else if (!drive_mirror_start(&h->mirror, &h->cockpit)) port_log("drive_hud: no mirror picture\n");  /* 0044bd64 */
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
    const int n = snprintf(dir, sizeof dir, "SIMDATA/MISC/");
    snprintf(h->root, sizeof h->root, "%s", data_root);
    DashFiles files;
    if (n < 0 || n >= (int)sizeof dir || dash_load_paths(&files, dir, (int16_t)resource_german(), HIGH_RES) != 0) return 0;
    h->bank = read_named(h->root, files.bank, (size_t)n);
    h->mmb = read_named(h->root, files.mmb, (size_t)n);
    if (h->bank == NULL || h->mmb == NULL) {
        port_log("drive_hud: no %s or %s\n", files.bank, files.mmb);
        drive_hud_free(h);
        return 0;
    }
    int parts = 1;
    for (int r = 0; r < DASH_PART_ROWS; r++)
        for (int c = 0; c < DASH_PART_COLUMNS; c++)
            parts = parts && (h->parts[r][c] = read_named(h->root, files.parts[r][c], (size_t)n)) != NULL;
    if (!parts) {
        port_log("drive_hud: no %s and the other parts, the mask from %s\n", files.parts[0][0], files.mmb);
        free_parts(h);
    }
    /* port: the distance unit's word comes from the bank of the units' language when that is not the language's */
    DashFiles units;
    if (resource_kmh() != resource_german() && dash_load_paths(&units, dir, (int16_t)resource_kmh(), HIGH_RES) == 0)
        h->units_bank = read_named(h->root, units.bank, (size_t)n);
    return 1;
}

void drive_hud_setup(DriveHud *h, RacePalette *p)
{
    if (h->bank == NULL) return;
    h->ready = dash_load_setup(&h->art, &h->replay, p, h->bank, h->mmb, HIGH_RES, 0) == 0;
    h->art.units_bank = h->units_bank;
    if (h->units_bank != NULL) dash_load_units(p, h->units_bank);   /* port: mapped with the language's bank */
    memcpy(h->art.parts, h->parts, sizeof h->parts);
    if (!h->ready) port_log("drive_hud: the mask bank lacks a shape the HUD needs\n");
}


void drive_hud_start(DriveHud *h, RacePalette *p)
{
    if (!h->ready) return;
    HudColours colours;  /* 00446bac */
    hud_colours(p, &colours);
    h->strip_colour = (uint32_t)colours.colours[HUD_COLOUR_BLACK];
    h->needle_colour = (uint32_t)colours.colours[0];
    h->road_colour = (uint32_t)colours.colours[HUD_COLOUR_WHITE];
    h->frame_colour = (uint32_t)colours.frame_colour;
    h->mirror.enclosed_colour = (uint32_t)colours.colours[HUD_COLOUR_GREY];             /* 0050b7c0 */
    h->mirror.property8_colour = (uint32_t)colours.colours[HUD_COLOUR_GREEN_DARK];      /* 0050b79c */
    memcpy(h->car_colours, colours.map_cars, sizeof h->car_colours);
    memcpy(h->shade, colours.remap, sizeof h->shade);  /* 004c6604 */
    h->palette = p;
    memset(&h->police, 0, sizeof h->police);
    memset(h->back, 0, sizeof h->back);
    h->back[0] = SHAPE_8_BIT;
    h->back[4] = DRIVE_BACK_W, h->back[6] = DRIVE_BACK_H;
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
    h->redraw = 0;
    h->view_changed = &h->unknown_511fec;
    h->unknown_511fec = 0;
    memset(&h->keys, 0, sizeof h->keys);
    h->keys.armed = 1;  /* 004b9460's start */
    h->keys.use_parts = 1;  /* 0044ca3c */
    h->parts_key = key_code((uint16_t)key_layout(PARTS_KEY_CONFIG, (int16_t)resource_german()));  /* 00426ddc */
    h->parts_pending = 0;
    /* port: the dash box's mode. The exe's is the map from the start (M hides it); with Gear lever or Hidden the race
     * starts as M had hidden it, and M brings the map back. */
    h->dash_box = port_extras_dash_box();
    h->keys.use_parts = dash_box_start_parts((DashBox)h->dash_box);
    h->parts_pending = h->keys.use_parts == 0;
    memset(h->map, 0, sizeof h->map);
    h->map[0] = SHAPE_8_BIT;
    h->map[4] = DRIVE_MAP_W, h->map[6] = DRIVE_MAP_H;
    h->map_blink = 0;
    drive_panel_start(&h->panel);
}

/* The camera's view row's frame (00511fe8). */
static HudFrame view_frame(const RaceState *race)
{
    const RaceCamera *camera = race->chase.camera;
    const ViewRow *row = camera != NULL ? camera->view_row : NULL;
    return row != NULL ? (HudFrame){ row->unknown_14[4], row->unknown_14[5], row->unknown_14[6], row->unknown_14[7] }
                       : (HudFrame){ 0, 0, 0, 0 };
}

void drive_hud_tiles(DriveHud *h, const RaceState *race)
{
    if (!h->ready) return;
    const QuadTarget t = { h->view + DRIVE_VIEW_HEADER, DRIVE_VIEW_W, 0, 0, DRIVE_VIEW_W, DRIVE_VIEW_H };
    const HudRedrawInputs in = {
        view_frame(race), 1, DRIVE_VIEW_W, DRIVE_VIEW_H, race->splits.gap, race->splits.gap_shown, HIGH_RES, h->strip_colour };
    hud_redraw(&t, &h->hud, &h->art, race, &in);
}

uint8_t *drive_hud_view(DriveHud *h, uint8_t *pixels)
{
    return h->ready ? h->view + DRIVE_VIEW_HEADER : pixels;
}

void drive_hud_clear(DriveHud *h)
{
    hud_clear_request(&h->hud);
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
    ((KeysCalls *)ctx)->h->parts_pending = 1;  /* 00420918, onto the frame (strip_draw, hud_top_row) */
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
    if (h->parts_key > 0 && h->parts_key < (int32_t)sizeof keys) keys[h->parts_key] = platform_key_down(h->parts_key) ? 1 : 0;
    KeysCalls calls = { h, race, strip };
    const HudKeysOps ops = { &calls, keys_labels, keys_parts_redraw, keys_take };
    /* no pad */
    const HudKeysInputs in = { keys, h->parts_key > 0 && h->parts_key < (int32_t)sizeof keys ? h->parts_key : 0, 0,
                               { 0, 0 }, 0, 0 };
    h->keys.view_changed = *h->view_changed;
    hud_keys(&ops, &h->hud, &h->keys, race, &in);
    *h->view_changed = h->keys.view_changed;  /* 00511fec: 999 after F9, 0044cc74 makes it 1 for the next frame */
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

static void cockpit_grab(void *ctx, uint8_t *back_shape)
{
    ((CockpitSurfaces *)ctx)->h->port_grab(back_shape);
}

static void cockpit_redraw_now(DriveHud *h, CockpitSurfaces *s)
{
    const CockpitRedrawOps ops = { s, cockpit_select, cockpit_labels, h->port_grab != NULL ? cockpit_grab : NULL };
    const CockpitRedrawInputs in = { 0, drive_hud_view_clip_inputs(s->screen.pitch, s->screen.clip_bottom), h->back, h->shade };
    cockpit_redraw(&ops, &h->hud, &h->cockpit, &h->cockpit_world, &in);
}

/* The frame's colours from the race palette's after 00476460 wrote it. */
static void rgb_from_palette(DriveHud *h)
{
    if (h->rgb == NULL || h->palette == NULL) return;
    for (int i = 0; i < RACE_PALETTE_COLOURS; i++)
        for (int k = 0; k < 3; k++) h->rgb[3 * i + k] = h->palette->screen_rgbx[4 * i + k];
}

void drive_hud_palette_write(void *hud, int32_t first, int32_t count, const uint8_t *colours)
{
    DriveHud *h = hud;
    if (h->palette == NULL) return;
    race_palette_write(h->palette, first, count, colours);
    rgb_from_palette(h);
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
    char path[ROOTED_PATH];
    const size_t dir = strlen(h->root) + 1 + strlen(h->misc_dir);
    snprintf(path, sizeof path, "%s/%s%s", h->root, h->misc_dir, name);
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
    const CockpitPoliceInputs in = { 0, race->stop_notice[0], (int16_t)resource_german(), 0, "",
                                     drive_hud_view_clip_inputs(s->screen.pitch, s->screen.clip_bottom) };
    const int had = h->police.screen_bank != NULL;
    if (cockpit_police(&ops, &h->cockpit, &h->police, &h->hud, &in) != 0 && !had)
        port_log("drive_hud: no police screen\n");
}

/* 004217a8's calls. */
static void reload_free_saved(void *ctx)
{
    DriveHud *h = ((CockpitSurfaces *)ctx)->h;
    resource_free(h->police.screen_bank);
    h->police.screen_bank = NULL, h->police.screen = NULL;
}

static void reload_none(void *ctx) { (void)ctx; }  /* 00464a30 (no back buffer here); port: 00473614 is platform only */

static void reload_load(void *ctx)
{
    CockpitSurfaces *s = ctx;
    DriveHud *h = s->h;
    size_t size = 0;
    h->dash_bank = read_sized(h->root, h->dash_path, h->dash_dir_length, &size);
    if (h->dash_bank == NULL || cockpit_load(&h->cockpit, &h->cockpit_world, &h->hud, s->race, &h->keys.dash_camera,
                                             h->dash_bank, size, h->dash_fmm, h->dash_stem, HIGH_RES, 0) != 0) {
        port_log("drive_hud: the dashboard did not load again\n");
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

/* 0042270c's calls: the view surface (004c2d90) and the frame (004c2da0
 * and the default both). The shapes are drawn only while the dashboard is
 * loaded: a reload that failed leaves them pointing into the freed bank.
 * 00421e40 and 00420710 into the strip and the map's copy run after it in
 * drive_hud_in_car for every view, so here they do nothing. */
typedef struct {
    CockpitSurfaces *s;
    const Car *car;
    int32_t redline;
    QuadTarget view, screen, *target;
} FrameCalls;

static int frame_shapes(const FrameCalls *f) { return f->s->h->cockpit_ready; }

static int32_t frame_rand(void *ctx, int32_t n) { return race_rand_below(((FrameCalls *)ctx)->s->race, n); }
static void frame_police(void *ctx) { FrameCalls *f = ctx; draw_police(f->s->h, f->s->race, f->s); }
static void frame_none(void *ctx) { (void)ctx; }  /* 00420710, 00421e40 (drive_hud_in_car) */
/* 00473614: the DirectDraw surface unlocked, 004c78b4's descriptor cleared, the display restored; the port's
 * surfaces need none of it (the next select sets the target). */
static void frame_restore(void *ctx) { (void)ctx; }

static void frame_reload(void *ctx)
{
    FrameCalls *f = ctx;
    if (f->s->h->dash_fmm != NULL) reload_cockpit(f->s->h, f->s->race, f->s);
}

static void frame_redraw(void *ctx)
{
    FrameCalls *f = ctx;
    if (frame_shapes(f)) cockpit_redraw_now(f->s->h, f->s);
}

static void frame_select(void *ctx, HudSurface s)
{
    FrameCalls *f = ctx;
    f->target = s == HUD_SURFACE_VIEW ? &f->view : &f->screen;
}

static void frame_clip(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom)
{
    FrameCalls *f = ctx;
    clip_set(f->target, left, right, top, bottom, f->target == &f->view ? DRIVE_VIEW_W : f->screen.pitch,
             f->target == &f->view ? DRIVE_VIEW_H : f->screen.clip_bottom);
}

static void frame_fill(void *ctx, uint32_t colour)
{
    const QuadTarget *t = ((FrameCalls *)ctx)->target;
    flat_fill_rect(t, t->clip_left, t->clip_top, t->clip_right - t->clip_left, t->clip_bottom - t->clip_top, colour);
}

static void frame_runs(void *ctx, int32_t runs)
{
    FrameCalls *f = ctx;
    blit_runs(f->target, DRIVE_VIEW_W, f->s->h->view, f->s->h->dash_fmm + runs);  /* relocated as 004511a4 does */
}

static void frame_mirror(void *ctx, int32_t x, int32_t y)
{
    FrameCalls *f = ctx;
    f->s->h->mirror_x = x, f->s->h->mirror_y = y;
    drive_mirror_place(&f->s->h->mirror, f->target, x, y);  /* 0044c060 */
}

static void frame_clip_box(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h)
{
    frame_clip(ctx, x, x + w, y, y + h);  /* 0044987c */
}

static void frame_place_skip(void *ctx, const uint8_t *shape)
{
    FrameCalls *f = ctx;
    if (frame_shapes(f) && shape != NULL) blit_shape_placed_transparent(f->target, shape);
}

static void frame_place(void *ctx, const uint8_t *shape)
{
    FrameCalls *f = ctx;
    if (frame_shapes(f) && shape != NULL) blit_shape_placed(f->target, shape);
}

static void frame_present(void *ctx)
{
    FrameCalls *f = ctx;
    blit_shape_placed(f->target, f->s->h->view);
}

static void frame_gauges(void *ctx)
{
    FrameCalls *f = ctx;
    DriveHud *h = f->s->h;
    if (!frame_shapes(f)) return;
    NeedleLine *line = &h->needle_line;
    line->left = h->colours.line_left, line->top = h->colours.line_top;
    line->right = h->colours.line_right, line->bottom = h->colours.line_bottom;
    const CockpitGaugeInputs in = { f->car->rpm, f->redline, f->car->local_velocity_z, f->car->model, HIGH_RES, (int16_t)resource_kmh(),
                                    h->colours.fade };
    cockpit_gauges(f->target, line, &h->cockpit, &in);
}

static void frame_gate(void *ctx)
{
    FrameCalls *f = ctx;
    DriveHud *h = f->s->h;
    if (!frame_shapes(f)) return;
    const CockpitSelectOps ops = { f->s, cockpit_select };
    const CockpitGateInputs in = { 0, h->keys.use_parts, drive_hud_view_clip_inputs(f->screen.pitch, f->screen.clip_bottom),
                                    dash_box_gate_always((DashBox)h->dash_box) };
    cockpit_gate(&ops, &h->hud, &h->cockpit, f->car, &in, &h->gate_state);
}

static void frame_lights(void *ctx)
{
    FrameCalls *f = ctx;
    DriveHud *h = f->s->h;
    if (frame_shapes(f))
        cockpit_lights(&f->screen, &h->cockpit, f->s->race->unknown_4b92d4, h->cockpit_world.unknown_4f9b48,
                       &h->hud.unknown_4f9a80[2]);
}

static void frame_map(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }  /* drive_hud_in_car's copy */

/* 0044d640's panels on the frame (drive_panel.c) */
typedef struct {
    DriveHud *h;
    DrivePanelDraw d;
} PanelCalls;

static void panel_dash(void *ctx) { PanelCalls *c = ctx; drive_panel_dash(&c->h->panel, &c->d); }
static void panel_replay(void *ctx) { PanelCalls *c = ctx; drive_panel_replay(&c->h->panel, &c->d); }

/* 0044d640 over the race's words; the views kept where the original keeps them (00513898 is the director's). */
static void draw_panel(DriveHud *h, RaceState *race, const QuadTarget *screen)
{
    PanelCalls c = { h, { screen, DRIVE_VIEW_W, DRIVE_VIEW_H, &h->art, &h->replay, &h->hud, race, h->director,
                          h->car_colours, h->mirror.property8_colour, h->strip_colour } };
    const RacePanelOps ops = { &c, panel_dash, panel_replay };
    const RacePanelInputs in = { race->phase, race->replay, race->viewed, race->player, race->unknown_513e6c,
                                 race->chase.kind };
    int32_t *redraw = h->director != NULL ? &h->director->unknown_513898 : &h->art.unknown_513888[DASH_REDRAW];
    RacePanelViews v = { h->art.unknown_513888[DASH_VIEW], *redraw, h->replay.camera, h->panel.panel.redraw };
    race_panel(&ops, &v, &in);
    h->art.unknown_513888[DASH_VIEW] = v.dash_view, *redraw = v.dash_redraw;
    h->replay.camera = v.replay_view, h->panel.panel.redraw = v.replay_redraw;
}

static void frame_panel(void *ctx) { FrameCalls *f = ctx; draw_panel(f->s->h, f->s->race, &f->s->screen); }

/* 0042270c; 1 when a stop notice was up (004214e4 drew it, nothing else this frame). */
static int draw_cockpit(DriveHud *h, CockpitSurfaces *s, const Car *car, int32_t redline)
{
    const int notice = s->race->stop_notice[0] != 0 || s->race->stop_notice[1] != 0;
    FrameCalls f = { s, car, redline, { h->view + DRIVE_VIEW_HEADER, DRIVE_VIEW_W, 0, 0, DRIVE_VIEW_W, DRIVE_VIEW_H },
                     s->screen, NULL };
    f.target = &f.screen;
    const CockpitFrameOps ops = { &f, frame_rand, frame_police, frame_reload, frame_redraw, frame_select, frame_clip,
                                  frame_fill, frame_runs, frame_restore, frame_mirror, frame_clip_box, frame_place_skip,
                                  frame_place, frame_place_skip, frame_present, frame_gauges, frame_gate, frame_lights,
                                  frame_none, frame_none, frame_map, frame_panel };
    uint8_t clear_view = 0;
    int32_t unknown_4b956c = 0;
    const CockpitFrameWords w = { &h->police.loading, &clear_view, &unknown_4b956c };
    const CockpitFrameInputs in = { 0, 0, HIGH_RES, 0, s->race->window_size, LIVE_RACE, car->model, h->keys.use_parts,
                                    h->strip_colour, s->screen.pitch, s->screen.clip_bottom, 0 };
    cockpit_frame(&ops, &h->hud, s->race, &h->cockpit, car, &w, &in);
    return notice;
}

static QuadTarget *map_select(void *ctx, HudSurface s)
{
    (void)s;  /* 00422ce0 selects only 004c2d9c */
    return ctx;
}

/* 00422ce0 into the map's surface, from the race's and the cockpit's words. */
void drive_hud_map(DriveHud *h, const RaceState *race)
{
    if (!h->ready || !h->cockpit_ready) return;
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

/* The strip's part of 00420710 and 00421e40: the keys and the top row into the strip, which then covers the frame's
 * top rows; 00420918's parts when the keys asked for them. */
static void strip_draw(DriveHud *h, RaceState *race, const Car *car, uint8_t *pixels, int width, int height)
{
    const QuadTarget t = { h->strip, DRIVE_HUD_W, 0, 0, DRIVE_HUD_W, DRIVE_HUD_H };
    draw_keys(h, race, &t);
    hud_draw_top_row(&t, &h->hud, &h->art, race, car, (int16_t)resource_kmh(), HIGH_RES, h->strip_colour);
    if (pixels == NULL) return;
    const int w = width < DRIVE_HUD_W ? width : DRIVE_HUD_W;
    for (int y = 0; y < DRIVE_HUD_H && y < height; y++)
        memcpy(pixels + (size_t)y * (size_t)width, h->strip + (size_t)y * DRIVE_HUD_W, (size_t)w);
    if (width == DRIVE_VIEW_W && height == DRIVE_VIEW_H && h->parts_pending) {
        QuadTarget screen = { pixels, width, 0, 0, width, height };
        const HudPartsInputs parts = { view_frame(race), 1, width, height, HIGH_RES, width, height };
        hud_parts_redraw(&screen, &h->art, &parts);
        h->parts_pending = 0;
    }
}

/* 00422598's calls (hud_frame) on the frame: 00420710 and 00421e40 split as strip_draw does them (the keys in
 * keys, the top row and the strip's copy in top_row); the pages, the default surface and 00473614 are the frame. */
typedef struct {
    CockpitSurfaces *s;
    const Car *car;
    int32_t redline;
    int width, height;
} HudCalls;

static void hud_police(void *ctx) { HudCalls *c = ctx; draw_police(c->s->h, c->s->race, c->s); }
static void hud_none(void *ctx) { (void)ctx; }
static void hud_select(void *ctx, HudSurface s) { (void)ctx, (void)s; }
static void hud_panel(void *ctx) { HudCalls *c = ctx; draw_panel(c->s->h, c->s->race, &c->s->screen); }

static void hud_reselect(void *ctx)
{
    const RaceState *race = ((HudCalls *)ctx)->s->race;
    if (race->view_user != NULL && race->chase.camera != NULL)
        race_view_reselect(race->view_user, race->chase.camera, race->window_size);
}

static void hud_reload(void *ctx)
{
    HudCalls *c = ctx;
    DriveHud *h = c->s->h;
    if (h->dash_fmm != NULL && h->cockpit.bank == NULL) reload_cockpit(h, c->s->race, c->s);
}

/* 004218d0 onto the frame (the default surface): the tiles, the labels, the frame round the view, the tach. */
static void hud_tiles(void *ctx)
{
    HudCalls *c = ctx;
    DriveHud *h = c->s->h;
    const HudRedrawInputs in = { view_frame(c->s->race), 1, c->width, c->height, c->s->race->splits.gap,
                                 c->s->race->splits.gap_shown, HIGH_RES, h->strip_colour };
    hud_redraw(&c->s->screen, &h->hud, &h->art, c->s->race, &in);
}

static void hud_mask(void *ctx, int32_t changed)
{
    HudCalls *c = ctx;
    drive_hud_mask(c->s->h, c->s->race, c->s->screen.pixels, c->width, c->height, changed);
}

static void hud_keys_op(void *ctx)
{
    HudCalls *c = ctx;
    const QuadTarget t = { c->s->h->strip, DRIVE_HUD_W, 0, 0, DRIVE_HUD_W, DRIVE_HUD_H };
    draw_keys(c->s->h, c->s->race, &t);
}

static void hud_top_row(void *ctx)
{
    HudCalls *c = ctx;
    DriveHud *h = c->s->h;
    const QuadTarget t = { h->strip, DRIVE_HUD_W, 0, 0, DRIVE_HUD_W, DRIVE_HUD_H };
    hud_draw_top_row(&t, &h->hud, &h->art, c->s->race, c->car, (int16_t)resource_kmh(), HIGH_RES, h->strip_colour);
    uint8_t *pixels = c->s->screen.pixels;
    for (int y = 0; y < DRIVE_HUD_H && y < c->height; y++)
        memcpy(pixels + (size_t)y * (size_t)c->width, h->strip + (size_t)y * DRIVE_HUD_W, DRIVE_HUD_W);
    if (h->parts_pending) {
        const HudPartsInputs parts = { view_frame(c->s->race), 1, c->width, c->height, HIGH_RES, c->width, c->height };
        hud_parts_redraw(&c->s->screen, &h->art, &parts);
        h->parts_pending = 0;
    }
}

static void hud_needle(void *ctx)
{
    HudCalls *c = ctx;
    DriveHud *h = c->s->h;
    hud_draw_needle(&c->s->screen, &h->needle, &h->art, c->car->rpm, c->redline, c->width, c->height, c->width, c->height,
                    HIGH_RES, h->needle_colour, NULL, NULL);
}

static void hud_map(void *ctx, int32_t x, int32_t y)
{
    HudCalls *c = ctx;
    if (c->s->h->cockpit_ready) blit_shape_at(&c->s->screen, c->s->h->map, x, y);  /* the map is drawn with a cockpit */
}

static CockpitSurfaces frame_surfaces(DriveHud *h, RaceState *race, uint8_t *pixels)
{
    return (CockpitSurfaces){ h, race, { pixels, DRIVE_VIEW_W, 0, 0, DRIVE_VIEW_W, DRIVE_VIEW_H },
                              { h->back + DRIVE_VIEW_HEADER, DRIVE_BACK_W, 0, 0, DRIVE_BACK_W, DRIVE_BACK_H }, { 0 } };
}

void drive_hud_outside(DriveHud *h, RaceState *race, const Car *car, int32_t redline, uint8_t *pixels, int32_t changed)
{
    if (!h->ready || car == NULL || pixels == NULL) return;
    CockpitSurfaces s = frame_surfaces(h, race, pixels);
    HudCalls c = { &s, car, redline, DRIVE_VIEW_W, DRIVE_VIEW_H };
    const HudFrameOps ops = { &c, hud_police, hud_reselect, hud_panel, hud_reload, hud_none, hud_select, hud_tiles,
                              hud_none, hud_none, hud_none, hud_mask, hud_keys_op, hud_top_row, hud_none, hud_needle,
                              hud_map };
    const HudFrameWords w = { &h->redraw, &h->police.loading, &race->stop_notice_tick };
    const HudFrameInputs in = { 0, race->stop_notice[0], race->stop_notice[1], 0, h->keys.use_parts, HIGH_RES };
    hud_frame(&ops, &w, &in, changed);
}

void drive_hud_in_car(DriveHud *h, RaceState *race, const Car *car, int32_t redline, uint8_t *pixels)
{
    if (!h->ready || car == NULL || pixels == NULL) return;
    if (!h->cockpit_ready) {  /* port guard: the original always has its dash loaded here */
        drive_hud_outside(h, race, car, redline, pixels, 0);
        return;
    }
    CockpitSurfaces s = frame_surfaces(h, race, pixels);
    if (draw_cockpit(h, &s, car, redline)) return;
    strip_draw(h, race, car, pixels, DRIVE_VIEW_W, DRIVE_VIEW_H);
    if (h->keys.use_parts != 0) blit_shape_at(&s.screen, h->map, DRIVE_MAP_X, DRIVE_MAP_Y);
}

void drive_hud_strip(DriveHud *h, RaceState *race)
{
    if (!h->ready) return;
    const Car *car = race->cars[race_slot(race->viewed)];
    if (car != NULL) strip_draw(h, race, car, NULL, 0, 0);
}

static void end_free_bank(void *ctx, uint8_t *bank)
{
    DriveHud *h = ctx;
    (void)bank;
    platform_free_file(h->dash_bank);
    h->dash_bank = NULL;
}

static void end_free_screen(void *ctx, uint8_t *bank)
{
    DriveHud *h = ctx;
    (void)bank;
    resource_free(h->police.screen_bank);
    h->police.screen = NULL;
}

static void end_free_fmm(void *ctx, const uint8_t *fmm)
{
    DriveHud *h = ctx;
    (void)fmm;
    platform_free_file(h->dash_fmm);
    h->dash_fmm = NULL;  /* the original keeps 004f9a04; nothing reads it before the next load */
}

static void end_none(void *ctx) { (void)ctx; }  /* the fade block and the inverse table are the port's own arrays */

void drive_hud_end(DriveHud *h, RaceState *race)
{
    static RacePalette scratch;  /* port guard: no race palette was kept */
    int32_t unknown_513e60 = 0, unknown_4b91c8 = 0;
    const CockpitTeardownWords w = { &unknown_513e60, &unknown_4b91c8 };
    const CockpitTeardownOps ops = { h, end_free_bank, end_free_screen, end_none, end_none, end_free_fmm };
    cockpit_teardown(&ops, &h->cockpit, &h->police, h->palette != NULL ? h->palette : &scratch, race, &w);
    drive_mirror_free(&h->mirror);  /* 0044bdbc */
    h->cockpit_ready = 0;
}

void drive_hud_free(DriveHud *h)
{
    platform_free_file(h->bank);
    platform_free_file(h->mmb);
    platform_free_file(h->units_bank);
    platform_free_file(h->dash_bank);
    platform_free_file(h->dash_fmm);
    free_parts(h);
    resource_free(h->police.screen_bank);
    h->police.screen_bank = NULL, h->police.screen = NULL;
    drive_mirror_free(&h->mirror);
    h->bank = h->mmb = h->units_bank = h->dash_bank = h->dash_fmm = NULL;
    h->cockpit_ready = 0;
    h->ready = 0;
}
