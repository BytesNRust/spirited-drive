#include "viewer/drive_hud.h"

#include "game/platform/platform.h"
#include "game/gfx/blit.h"
#include "game/gfx/flat.h"
#include "game/race/dash_load.h"
#include "game/race/hud_mask.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { HIGH_RES = 1, ENGLISH = 0, SHAPE_8_BIT = 0x7b };

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

/* TODO(00446880): the colour search; here the entry nearest (r, g, b) by
 * squared distance. */
static uint32_t nearest_colour(const RacePalette *p, int r, int g, int b)
{
    uint32_t best = 0, best_d = 0xffffffffu;
    for (uint32_t i = 0; i < RACE_PALETTE_COLOURS; i++) {
        const uint8_t *c = p->screen_rgbx + 4 * i;
        const int dr = c[0] - r, dg = c[1] - g, db = c[2] - b;
        const uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db);
        if (d < best_d) best = i, best_d = d;
    }
    return best;
}

void drive_hud_start(DriveHud *h, RacePalette *p)
{
    if (!h->ready) return;
    /* TODO(00447098): the queued banks' remap; here the mask bank alone. */
    race_palette_remap_track_bank(p, h->bank, h->art.palette);
    h->strip_colour = nearest_colour(p, 0, 0, 0);
    h->needle_colour = nearest_colour(p, 0xff, 0, 0);  /* 00446bac: 00446880(0x3f, 0, 0) */
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
    h->presented = 0;
}

uint8_t *drive_hud_view(DriveHud *h, uint8_t *pixels)
{
    return h->ready ? h->view + DRIVE_VIEW_HEADER : pixels;
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

static void mask_view_clip(void *ctx)
{
    (void)ctx;  /* TODO(0041e3f4) */
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
    if (!h->presented) {
        hud_mask_draw(&ops, &h->art, &h->hud, &clear_view, &in, 1);
        h->presented = 1;
    }
    hud_mask_draw(&ops, &h->art, &h->hud, &clear_view, &in, 0);
}

void drive_hud_draw(DriveHud *h, RaceState *race, int32_t redline, uint8_t *pixels, int width, int height)
{
    if (!h->ready) return;
    const Car *car = race->cars[race_slot(race->viewed)];
    if (car == NULL) return;
    if (pixels != NULL && width == DRIVE_VIEW_W && height == DRIVE_VIEW_H) draw_mask(h, race, pixels, width, height);
    const QuadTarget t = { h->strip, DRIVE_HUD_W, 0, 0, DRIVE_HUD_W, DRIVE_HUD_H };
    if (h->hud.lights_until < race->tick) {  /* 00420710 */
        hud_draw_labels(&t, &h->hud, &h->art, race, race->splits.gap, race->splits.gap_shown, HIGH_RES,
                        h->strip_colour);
        h->hud.waiting = 0;
    }
    hud_draw_top_row(&t, &h->hud, &h->art, race, car, ENGLISH, HIGH_RES, h->strip_colour);
    if (pixels == NULL) return;
    const int w = width < DRIVE_HUD_W ? width : DRIVE_HUD_W;
    for (int y = 0; y < DRIVE_HUD_H && y < height; y++)
        memcpy(pixels + (size_t)y * (size_t)width, h->strip + (size_t)y * DRIVE_HUD_W, (size_t)w);
    QuadTarget screen = { pixels, width, 0, 0, width, height };
    hud_draw_needle(&screen, &h->needle, &h->art, car->rpm, redline, width, height, width, height, HIGH_RES,
                    h->needle_colour);
}

void drive_hud_free(DriveHud *h)
{
    platform_free_file(h->bank);
    platform_free_file(h->mmb);
    h->bank = h->mmb = NULL;
    h->ready = 0;
}
