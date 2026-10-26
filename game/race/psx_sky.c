/* port: see psx_sky.h. The files come from outside: every read is checked. */
#include "game/race/psx_sky.h"
#include "formats/psx_bigf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LINE = 128, FIELDS = 14 };

/* The next value line of the text at *at: its text without the line end, 0 at the end. */
static int next_line(const uint8_t *file, size_t size, size_t *at, char line[LINE])
{
    while (*at < size) {
        size_t end = *at;
        while (end < size && file[end] != '\n' && file[end] != '\r') end++;
        const size_t n = end - *at < LINE - 1 ? end - *at : LINE - 1;
        memcpy(line, file + *at, n);
        line[n] = '\0';
        while (end < size && (file[end] == '\n' || file[end] == '\r')) end++;
        *at = end;
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '#' && *p != '\0') return 1;
    }
    return 0;
}

int psx_sky_inf_parse(const uint8_t *file, size_t size, PsxSkyInf *out)
{
    if (file == NULL) return 0;
    PsxSkyInf inf;
    memset(&inf, 0, sizeof inf);
    size_t at = 0;
    char line[LINE];
    int ints[4] = { 0 };
    for (int field = 0; field < FIELDS && next_line(file, size, &at, line); field++) {
        if (field < PSX_SKY_GEOMETRY) {
            inf.geometry[field] = (float)atof(line);
        } else if (field < PSX_SKY_GEOMETRY + 2) {
            int *c = field == PSX_SKY_GEOMETRY ? inf.top : inf.bottom;
            if (sscanf(line, "%d %d %d", &c[0], &c[1], &c[2]) != 3) return 0;
        } else {
            ints[field - PSX_SKY_GEOMETRY - 2] = atoi(line);
        }
        inf.fields = field + 1;
    }
    if (inf.fields < PSX_SKY_GEOMETRY + 2) return 0;
    inf.dither = ints[0], inf.blend = ints[1], inf.ring_y = ints[2], inf.ring_height = ints[3];
    *out = inf;
    return 1;
}

int psx_sky_panorama(const uint8_t *psh, size_t size, PsxSky *out)
{
    out->width = out->height = 0, out->rgb = NULL;
    PsxImage8 tile[PSX_SKY_TILES];
    for (int i = 0; i < PSX_SKY_TILES; i++) {
        char name[5] = "hrz0";
        name[3] = (char)('0' + i);
        if (!psx_shpp_image8(psh, size, name, &tile[i])) return 0;
        if (tile[i].width != tile[0].width || tile[i].height != tile[0].height) return 0;
    }
    const int w = tile[0].width, h = tile[0].height;
    const size_t row = (size_t)w * PSX_SKY_TILES;
    uint8_t *rgb = malloc(row * (size_t)h * 3);
    if (rgb == NULL) return 0;
    for (int i = 0; i < PSX_SKY_TILES; i++)
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                psx_clut_rgb(tile[i].clut, tile[i].pixels[(size_t)y * w + x], rgb + 3 * (y * row + (size_t)i * w + x));
    out->width = (int)row, out->height = h, out->rgb = rgb;
    return 1;
}

void psx_sky_free(PsxSky *sky)
{
    free(sky->rgb);
    sky->rgb = NULL, sky->width = sky->height = 0;
}

uint32_t psx_sky_checksum(const uint8_t *bytes, size_t count)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < count; i++) sum = sum * 31 + bytes[i] + 1;
    return sum;
}
