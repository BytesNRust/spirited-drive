#include "viewer/drive_light.h"
#include "viewer/ported_view.h"
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
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGS");
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("psx_light: no %s, the ambient stays neutral\n", path);
        return;
    }
    PsxLgs lgs;
    if (psx_lgs_parse(file, size, &lgs)) psx_lgs_tint(&lgs, tint);
    else port_log("psx_light: %s does not read, the ambient stays neutral\n", path);
    platform_free_file(file);
}

void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    ported_view_set_light(NULL, NULL);
    psx_light_free(&light);
    tint[0] = tint[1] = tint[2] = 1.0f;
    if (!enabled || psx_dir == NULL || psx_dir[0] == '\0') return;
    const char letter = LETTERS[time >= PSX_TIME_DAY && time <= PSX_TIME_DUSK ? time : PSX_TIME_DAY];
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

void drive_light_palette_half(const uint8_t palette[768], uint8_t out[768])
{
    uint8_t full[768];
    psx_tint_palette(palette, tint, full);
    for (int i = 0; i < 768; i++) out[i] = (uint8_t)((palette[i] + full[i] + 1) / 2);
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
