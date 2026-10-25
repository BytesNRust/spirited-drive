#include "viewer/drive_light.h"
#include "viewer/ported_view.h"
#include "viewer/psx_sky_draw.h"
#include "game/race/psx_sky.h"
#include "game/platform/platform.h"
#include "game/race/psx_lgs.h"
#include "game/race/psx_light.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LIGHT_PATH = 320, LIGHT_TRACK = 16 };

static const char LETTERS[] = { 'B', 'A', 'C' };   /* by PSX_TIME_*: day, dawn, dusk */

static PsxLight light;
static float tint[3] = { 1, 1, 1 };
static PsxLgs ambient;
static int ambient_ok;   /* ambient holds the LGS read */
static uint32_t ambient_nodes;
static int32_t dash_node = -1;   /* the node of the car the dash is of; -1 none */
enum { DASH_STEP = 32 };
static PsxSkyDraw sky_draw;

/* the files' path: <psx_dir>/Z<TRACK><letter>.<ext> */
static void file_path(char *out, size_t size, const char *psx_dir, const char *track, char letter, const char *ext)
{
    char upper[LIGHT_TRACK];
    size_t i = 0;
    for (; track[i] != '\0' && i < LIGHT_TRACK - 1; i++) upper[i] = (char)toupper((unsigned char)track[i]);
    upper[i] = '\0';
    snprintf(out, size, "%s/Z%s%c.%s", psx_dir, upper, letter, ext);
}

/* the LGS's ambient colour as a tint, neutral when the file is missing or does not read */
static void load_tint(const char *psx_dir, const char *track, char letter)
{
    tint[0] = tint[1] = tint[2] = 1.0f;
    ambient_ok = 0;
    dash_node = -1;
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGS");
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("psx_light: no %s, the ambient stays neutral\n", path);
        return;
    }
    if (psx_lgs_parse(file, size, &ambient)) ambient_ok = 1, psx_lgs_tint(&ambient, tint);
    else port_log("psx_light: %s does not read, the ambient stays neutral\n", path);
    platform_free_file(file);
}

/* dawn and dusk: the PS1's own sky (Z<TRACK><A|C>.INF and .PSH) for the race; the day, a missing or unreadable file, or only the INF
 * leave the track's own sky (tinted with the world) */
static void load_sky(const char *psx_dir, const char *track, char letter)
{
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "PSH");
    size_t psh_size = 0;
    uint8_t *psh = platform_read_file(path, &psh_size);
    if (psh == NULL) {
        port_log("psx_sky: no %s, the track's sky stays\n", path);
        return;
    }
    PsxSky sky;
    const int panorama = psx_sky_panorama(psh, psh_size, &sky);
    platform_free_file(psh);
    if (!panorama) {
        port_log("psx_sky: %s has no horizon tiles hrz0..hrz7, the track's sky stays\n", path);
        return;
    }
    PsxSkyInf inf = { { 0 }, { 0 }, { 0 }, 0, 0, 0, 0, 0 };
    file_path(path, sizeof path, psx_dir, track, letter, "INF");
    size_t inf_size = 0;
    uint8_t *text = platform_read_file(path, &inf_size);
    const int gradient = text != NULL && psx_sky_inf_parse(text, inf_size, &inf);
    if (text != NULL) platform_free_file(text);
    if (!gradient) port_log("psx_sky: %s does not read, no gradient above the horizon\n", path);
    if (psx_sky_draw_make(&sky_draw, &sky, &inf)) {
        ported_view_set_psx_sky(&sky_draw);
        port_log("psx_sky: %dx%d panorama, gradient %d,%d,%d to %d,%d,%d\n", sky.width, sky.height, inf.top[0], inf.top[1], inf.top[2],
                 inf.bottom[0], inf.bottom[1], inf.bottom[2]);
    }
    psx_sky_free(&sky);
}

void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    ported_view_set_soft_shadow(port_psx_shadow());   /* its own flag: no light or PS1 files needed */
    ported_view_set_light(NULL, NULL);
    ported_view_set_psx_ambient(NULL, 0, NULL);
    ported_view_set_psx_sky(NULL);
    psx_sky_draw_free(&sky_draw);
    psx_light_free(&light);
    tint[0] = tint[1] = tint[2] = 1.0f;
    if (!enabled || psx_dir == NULL || psx_dir[0] == '\0') return;
    const char letter = LETTERS[time >= PSX_TIME_DAY && time <= PSX_TIME_DUSK ? time : PSX_TIME_DAY];
    if (time != PSX_TIME_DAY) load_sky(psx_dir, track, letter);
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGT");
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("psx_light: no %s\n", path);
        return;
    }
    PsxLight parsed = { 0 };
    const int ok = psx_light_parse(file, size, ring_nodes, &parsed);
    platform_free_file(file);
    if (!ok) {
        port_log("psx_light: %s does not fit the track (%u nodes)\n", path, ring_nodes);
        return;
    }
    light = parsed;
    load_tint(psx_dir, track, letter);
    ambient_nodes = ring_nodes;
    if (ambient_ok) ported_view_set_psx_ambient(&ambient, ring_nodes, tint);
    ported_view_set_light(&light, NULL);   /* the tint goes to the palette (drive_light_palette), not through the polygons' tables */
    port_log("psx_light: %s, %u nodes, tint %.2f %.2f %.2f\n", path, light.nodes, tint[0], tint[1], tint[2]);
}

void drive_light_palette(const uint8_t palette[768], uint8_t out[768])
{
    psx_tint_palette(palette, tint, out);
}

/* the 3D as it was when it was done, and the mask of a shown frame against it */
static uint8_t *world_frame, *world_mask;
static size_t world_size;

void drive_light_world_done(const uint8_t *frame, size_t count)
{
    if (tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f) return;
    if (world_size != count) {
        uint8_t *f = realloc(world_frame, count), *m = realloc(world_mask, count);
        if (f != NULL) world_frame = f;
        if (m != NULL) world_mask = m;
        if (f == NULL || m == NULL) {
            world_size = 0;
            return;
        }
        world_size = count;
    }
    memcpy(world_frame, frame, count);
}

static int rect[4], rect_set, rect_width;

/* the 2D layer's classes (drive_light.h) per layer pixel, and the output's size against it */
static uint8_t *class_layer;
static size_t class_size;
static int class_width, class_scale = 1;

void drive_light_tint(float out[3]) { memcpy(out, tint, sizeof tint); }

int drive_light_active(void) { return tint[0] != 1.0f || tint[1] != 1.0f || tint[2] != 1.0f; }

void drive_light_world_class(const uint8_t *layer, int width, int height, int scale)
{
    const size_t count = (size_t)width * (size_t)height;
    if (!drive_light_active() || layer == NULL) {
        class_width = 0;
        return;
    }
    if (class_size != count) {
        uint8_t *p = realloc(class_layer, count);
        if (p == NULL) {
            class_width = 0;
            return;
        }
        class_layer = p, class_size = count;
    }
    memcpy(class_layer, layer, count);
    class_width = width, class_scale = scale < 1 ? 1 : scale;
}

void drive_light_dash_node(int32_t node) { dash_node = node; }

/* the dash's art: half the world's tint, as the dawn / dusk base, times the ambient at the car's segment over the world's tint */
void drive_light_palette_half(const uint8_t palette[768], uint8_t out[768])
{
    uint8_t full[768];
    psx_tint_palette(palette, tint, full);
    for (int i = 0; i < 768; i++) out[i] = (uint8_t)((palette[i] + full[i] + 1) / 2);
    if (!ambient_ok || dash_node < 0 || ambient_nodes < 4) return;
    float ratio[3];
    psx_lgs_ratio(&ambient, (int)(((uint32_t)dash_node & (ambient_nodes - 1u)) >> 2), (int)(ambient_nodes >> 2), tint, DASH_STEP, ratio);
    if (ratio[0] == 1.0f && ratio[1] == 1.0f && ratio[2] == 1.0f) return;
    uint8_t base[768];
    memcpy(base, out, sizeof base);
    psx_tint_palette(base, ratio, out);
}

void drive_light_world_rect(int width, int x, int y, int w, int h)
{
    rect[0] = x, rect[1] = y, rect[2] = w, rect[3] = h, rect_width = width, rect_set = w > 0 && h > 0;
}

const uint8_t *drive_light_world_mask(const uint8_t *shown, size_t count)
{
    if ((tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f) || world_size != count || world_frame == NULL) return NULL;
    psx_world_mask(shown, world_frame, count, world_mask);
    if (class_width > 0 && rect_width == class_width * class_scale && count % (size_t)rect_width == 0) {
        /* the 2D layer's own marks over the diff's: the dash's art half, the HUD and the gauges not at all */
        const size_t rows = count / (size_t)rect_width;
        for (size_t y = 0; y < rows; y++) {
            const uint8_t *row = class_layer + (y / (size_t)class_scale) * (size_t)class_width;
            for (size_t x = 0; x < (size_t)rect_width; x++) {
                const uint8_t c = row[x / (size_t)class_scale];
                if (c == PSX_CLASS_DASH) world_mask[y * (size_t)rect_width + x] = 2;
                else if (c == PSX_CLASS_HUD) world_mask[y * (size_t)rect_width + x] = 0;
            }
        }
    }
    if (rect_set && rect_width > 0 && count % (size_t)rect_width == 0)
        psx_mask_rect(world_mask, rect_width, (int)(count / (size_t)rect_width), rect[0], rect[1], rect[2], rect[3]);
    return world_mask;
}
