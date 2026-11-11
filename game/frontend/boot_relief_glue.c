/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: reads the boot relief's sources (boot_relief.h) off the player's CD and the region map from the port's data, and composes it. */
#include "game/frontend/boot_relief.h"

#include "formats/fsh.h"
#include "formats/png.h"
#include "game/sys/art_cache.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = BOOT_RELIEF_W, H = BOOT_RELIEF_H, N = W * H };

static const char MAP_FILE[] = "boot/boot_bg_map.png";

/* entry `tag` of art file `name` (w x h at least; the crop at (cx, cy) of size cw x ch) as RGB through `pal_file`'s !pal (NULL: its own) */
static int art_rgb(const char *name, const char tag[4], const char *pal_file, int cx, int cy, int cw, int ch, uint8_t *rgb)
{
    static const char PAL[4] = { '!', 'p', 'a', 'l' };
    size_t size = 0, psize = 0;
    uint8_t *data = resource_load_art(name, &size), *pdata = pal_file != NULL ? resource_load_art(pal_file, &psize) : NULL, pal[768];
    Fsh f, pf;
    FshEntry e, pe;
    uint32_t at;
    int ok = data != NULL && fsh_open(&f, data, size) == FSH_OK && fsh_find(&f, tag, &at) == FSH_OK && fsh_entry(&f, at, &e) == FSH_OK &&
             e.type == FSH_TYPE_BITMAP && e.width >= cx + cw && e.height >= cy + ch && e.payload_size >= (size_t)e.width * e.height;
    if (ok && pal_file != NULL)
        ok = pdata != NULL && fsh_open(&pf, pdata, psize) == FSH_OK && fsh_find(&pf, PAL, &at) == FSH_OK && fsh_entry(&pf, at, &pe) == FSH_OK &&
             fsh_palette_rgb(&pe, pal) == FSH_OK;
    else if (ok)
        ok = fsh_find(&f, PAL, &at) == FSH_OK && fsh_entry(&f, at, &pe) == FSH_OK && fsh_palette_rgb(&pe, pal) == FSH_OK;
    if (ok)
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++) memcpy(rgb + 3 * (y * cw + x), pal + 3 * e.payload[(size_t)(cy + y) * e.width + cx + x], 3);
    if (!ok) port_log("boot: no \"%.4s\" in %s on the CD\n", tag, name);
    resource_free(data);
    resource_free(pdata);
    return ok;
}

static uint8_t *read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    long n = 0;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n > 0 && fseek(f, 0, SEEK_SET) == 0) data = malloc((size_t)n);
    if (data != NULL && fread(data, 1, (size_t)n, f) == (size_t)n) *size = (size_t)n;
    else free(data), data = NULL;
    fclose(f);
    return data;
}

/* the first of `relative`'s candidates in the port's data (data_path.h) that is there, read whole; NULL when none is */
static uint8_t *read_data(const char *relative, size_t *size)
{
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(relative, found);
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++)
        if (found[i][0] != '\0' && data_path_exists(found[i])) return read_file(found[i], size);
    *size = 0;
    return NULL;
}

void boot_relief_key(ArtCacheKey *key)
{
    static const char *const CD[] = { "title.qfs", "iart/instunin.qfs", "iart/instinst.qfs", "iart/instvido.qfs", "control/dia1.qfs", "control/central.qfs" };
    size_t size = 0;
    uint8_t *data = read_data(MAP_FILE, &size);
    art_cache_key_add(key, data, size);
    free(data);
    for (size_t i = 0; i < sizeof CD / sizeof CD[0]; i++) {
        data = resource_load_art(CD[i], &size);
        art_cache_key_add(key, data, size);
        resource_free(data);
    }
}

/* the region map, a W x H grey PNG of BOOT_MAP_* values, from the port's data */
static int read_map(uint8_t *map)
{
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(MAP_FILE, found);
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) {
        if (found[i][0] == '\0' || !data_path_exists(found[i])) continue;
        size_t size = 0;
        uint8_t *file = read_file(found[i], &size);
        PngImage img;
        const int read = file != NULL && png_read(file, size, &img) == PNG_OK;
        free(file);
        if (!read) continue;
        const int fits = img.width == W && img.height == H;
        for (int p = 0; fits && p < N; p++) map[p] = img.indexed ? img.pixels[p] : img.pixels[4 * p];
        png_free(img.pixels);
        if (fits) return 1;
        port_log("boot: %s is not %dx%d\n", found[i], W, H);
    }
    port_log("boot: no %s in the port's data\n", MAP_FILE);
    return 0;
}

int boot_relief_load(uint8_t rgb[BOOT_RELIEF_W * BOOT_RELIEF_H * 3])
{
    static const char BGND[4] = { 'b', 'g', 'n', 'd' }, PHOTO[4] = { '0', '0', '0', '0' };
    uint8_t *title = malloc(3 * N), *screens = malloc(3 * (size_t)N * BOOT_RELIEF_SCREENS), *map = malloc(N);
    uint8_t photo[3 * BOOT_PHOTO_W * BOOT_PHOTO_H];
    int ok = title != NULL && screens != NULL && map != NULL && read_map(map) && art_rgb("title.qfs", BGND, NULL, 0, 0, W, H, title) &&
             art_rgb("control/dia1.qfs", PHOTO, "control/central.qfs", BOOT_PHOTO_CROP_X, BOOT_PHOTO_CROP_Y, BOOT_PHOTO_W, BOOT_PHOTO_H, photo);
    for (int k = 0; ok && k < BOOT_RELIEF_SCREENS; k++) ok = art_rgb(BOOT_RELIEF_SCREEN_NAMES[k], BGND, NULL, 0, 0, W, H, screens + 3 * (size_t)N * k);
    if (ok) {
        const BootReliefSources in = { title, { screens, screens + 3 * (size_t)N, screens + 6 * (size_t)N }, photo, map };
        ok = boot_relief_compose(&in, rgb);
        if (!ok) port_log("boot: the relief could not be composed\n");
    }
    free(title), free(screens), free(map);
    return ok;
}
