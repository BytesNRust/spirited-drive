/* port (dev, env only): TNFS_DASH_DUMP, the dash's 2D layer written for tools/dash_mask_psd.py. Split from drive_wide.c. */
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "game/gfx/blit.h"
#include "game/sys/backdrop.h"
#include "viewer/layer_compose.h"
#include "viewer/class_cache.h"
#include "viewer/drive_light.h"
#include "viewer/dash_box.h"
#include "game/sys/bands.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/race/car_draw.h"
#include "game/race/dash_color.h"
#include "game/race/dash_mask.h"
#include "game/race/wiper.h"
#include "viewer/wiper_scene.h"
#include "viewer/ported_view.h"
#include "game/race/tach_dim.h"
#include "game/race/wheel_front.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "viewer/drive_wide_private.h"

/* a 640 x 480 picture of palette indices as <dir>/<stem>.<what>.pgm (dash_dump) */
static void dash_dump_pgm(const char *dir, const char *stem, const char *what, const uint8_t *pixels)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.%s.pgm", dir, stem, what);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fprintf(f, "P5\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    fwrite(pixels, 1, (size_t)SCREEN_W * SCREEN_H, f);
    fclose(f);
}

/* port (dev, env only): TNFS_DASH_DUMP=<dir> writes each dash's 2D layer as <dir>/<STEM>.ppm (the dash art in the track's palette, the windshield black, and black too
 * wherever the steering wheel is in some pose: the wheel is drawn over the dash later, so the template does not hold it; wheel_front_zone) and a draft mask
 * <dir>/<STEM>.mask.png (the two gauge ellipses as DIAL), once per dash, for tools/dash_mask_psd.py's drafts */
/* the arrays dash_dump's parts share: the wheel's zone, the dials over it, and the dash with the wheel blanked */
static uint8_t dump_zone[SCREEN_W * SCREEN_H], dump_dials[SCREEN_W * SCREEN_H], dump_out[SCREEN_W * SCREEN_H];

/* <STEM>.ppm: the dash with the wheel blanked and the dials back over it, in the lit palette */
static void dash_dump_picture(const char *path, const Cockpit *k, const uint8_t *art)
{
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) {
            const int i = y * SCREEN_W + x;
            const int dial = dump_dials[i] != LAYER_KEY && in_gauge(k, x, y);
            dump_out[i] = dump_zone[i] ? (dial ? dump_dials[i] : LAYER_KEY) : art[i];
        }
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
            const uint8_t v = art[i];
            static const uint8_t black[3] = { 0, 0, 0 };
            fwrite(dump_out[i] == LAYER_KEY ? black : lit_palette + 3 * dump_out[i], 1, 3, f);
            (void)v;
        }
        fclose(f);
    }
}

/* the straight-ahead wheel, for a guide layer: the stored art where the wheel is in some pose (<STEM>.guide.ppm) and where that is (<STEM>.zone.pgm) */
static void dash_dump_guide(const char *dir, const char *done, const uint8_t *art)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.guide.ppm", dir, done);
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
            static const uint8_t black[3] = { 0, 0, 0 };
            fwrite(dump_zone[i] && art[i] != LAYER_KEY ? lit_palette + 3 * art[i] : black, 1, 3, f);
        }
        fclose(f);
    }
    snprintf(path, sizeof path, "%s/%s.zone.pgm", dir, done);
    if ((f = fopen(path, "wb")) != NULL) {
        fprintf(f, "P5\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fputc(dump_zone[i] && art[i] != LAYER_KEY ? 255 : 0, f);
        fclose(f);
    }
}

/* the inputs of tools/dash_rebuild.py, which gives the layer the dash behind the wheel from the steering frames: the art's palette indices (<STEM>.art.pgm), the 'w' frames'
 * ('w'l and 'w'r, each steering frame: <STEM>.w<row>_<frame>.pgm, 255 where a frame draws nothing), the dials over the wheel (<STEM>.dials.pgm, 255 elsewhere) and the palette (<STEM>.pal) */
static void dash_dump_rebuild_inputs(const char *dir, const char *done, const Cockpit *k, const uint8_t *art)
{
    dash_dump_pgm(dir, done, "art", art);
    static uint8_t dial_only[SCREEN_W * SCREEN_H];
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        const int x = i % SCREEN_W, y = i / SCREEN_W;
        const int dial = dump_dials[i] != LAYER_KEY && in_gauge(k, x, y);
        dial_only[i] = dial ? dump_dials[i] : 255;
    }
    dash_dump_pgm(dir, done, "dials", dial_only);
    for (int row = 0; row < 2; row++)
        for (int frame = 0; frame < COCKPIT_FRAMES; frame++) {
            const uint8_t *shape = k->frames[row][frame];
            if (shape == NULL) continue;
            static uint8_t pic[SCREEN_W * SCREEN_H];
            for (int y = 0; y < SCREEN_H; y++)
                for (int x = 0; x < SCREEN_W; x++) pic[y * SCREEN_W + x] = wheel_front_pixel(shape, x, y) ? shape[0x10 + (y - (int16_t)(shape[0xe] | shape[0xf] << 8)) * (int16_t)(shape[4] | shape[5] << 8) + (x - (int16_t)(shape[0xc] | shape[0xd] << 8))] : 255;
            char what[16];
            snprintf(what, sizeof what, "w%d_%d", row, frame);
            dash_dump_pgm(dir, done, what, pic);
        }
}

/* <STEM>.pal, and <STEM>.mask.png: the draft of a hand-made mask (the dials' marks, else automatic) */
static void dash_dump_palette_and_mask(const char *dir, const char *done, const Cockpit *k)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.pal", dir, done);
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(lit_palette, 1, 768, f);
        fclose(f);
    }
    static uint8_t draft[SCREEN_W * SCREEN_H];
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            draft[y * SCREEN_W + x] = in_dial(k, x, y) && dump_out[y * SCREEN_W + x] != LAYER_KEY ? DASH_MASK_DIAL : DASH_MASK_AUTO;
    uint8_t *png = NULL;
    size_t len = 0;
    snprintf(path, sizeof path, "%s/%s.mask.png", dir, done);
    f = fopen(path, "wb");
    if (f != NULL && dash_mask_write(draft, SCREEN_W, SCREEN_H, &png, &len)) fwrite(png, 1, len, f);
    if (f != NULL) fclose(f);
    free(png);
}

void dash_dump(const Cockpit *k, const uint8_t *art)
{
    const char *dir = getenv("TNFS_DASH_DUMP");
    static char done[sizeof drive_hud.dash_stem];
    if (dir == NULL || lit_palette == NULL || strcmp(done, drive_hud.dash_stem) == 0) return;
    snprintf(done, sizeof done, "%s", drive_hud.dash_stem);
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, done);
    wheel_front_zone(k, art, dump_zone);
    /* the dials the game draws over the 'w' frame inside the gauge ellipses ('tach', 'sped'): they come back over the blanked wheel */
    memset(dump_dials, LAYER_KEY, sizeof dump_dials);
    const QuadTarget dt = { dump_dials, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H };
    if (k->tach != NULL) blit_shape_at_transparent(&dt, k->tach, 0, 0);
    if (k->sped != NULL) blit_shape_at_transparent(&dt, k->sped, 0, 0);
    dash_dump_picture(path, k, art);
    dash_dump_guide(dir, done, art);
    dash_dump_rebuild_inputs(dir, done, k, art);
    dash_dump_palette_and_mask(dir, done, k);
}
