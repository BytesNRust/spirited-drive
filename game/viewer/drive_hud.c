#include "viewer/drive_hud.h"

#include "game/platform/platform.h"
#include "game/race/dash_load.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { HIGH_RES = 1, ENGLISH = 0 };

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

/* TODO(00446880): the colour search for black; here the darkest entry. */
static uint32_t nearest_black(const RacePalette *p)
{
    uint32_t best = 0, best_sum = 0xffffffffu;
    for (uint32_t i = 0; i < RACE_PALETTE_COLOURS; i++) {
        const uint8_t *c = p->screen_rgbx + 4 * i;
        const uint32_t sum = (uint32_t)c[0] + c[1] + c[2];
        if (sum < best_sum) best = i, best_sum = sum;
    }
    return best;
}

void drive_hud_start(DriveHud *h, RacePalette *p)
{
    if (!h->ready) return;
    /* TODO(00447098): the queued banks' remap; here the mask bank alone. */
    race_palette_remap_track_bank(p, h->bank, h->art.palette);
    h->strip_colour = nearest_black(p);
    memset(&h->hud, 0, sizeof h->hud);
    hud_reset(&h->hud);
    h->hud.mode = 0;
    h->hud.redraw = 1;
    memset(h->strip, (int)h->strip_colour, sizeof h->strip);
}

void drive_hud_draw(DriveHud *h, RaceState *race, uint8_t *pixels, int width, int height)
{
    if (!h->ready) return;
    const Car *car = race->cars[race_slot(race->viewed)];
    if (car == NULL) return;
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
}

void drive_hud_free(DriveHud *h)
{
    platform_free_file(h->bank);
    platform_free_file(h->mmb);
    h->bank = h->mmb = NULL;
    h->ready = 0;
}
