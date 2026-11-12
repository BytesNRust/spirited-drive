/* port: see port_icon_ours.h. */
#include "game/sys/port_icon_ours.h"

#include "formats/png.h"
#include "game/sys/data_path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int SIDES[PORT_ICON_SIZES] = { 32, 48, 64, 128, 256 };
enum { MAX_FILE = 4 << 20 };

static uint8_t *read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    long n = 0;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n > 0 && n <= MAX_FILE && fseek(f, 0, SEEK_SET) == 0) data = malloc((size_t)n);
    if (data != NULL && fread(data, 1, (size_t)n, f) == (size_t)n) *size = (size_t)n;
    else free(data), data = NULL;
    fclose(f);
    return data;
}

/* the RGBA of a PNG of `side` x `side`, or NULL; an indexed one is opaque */
static uint8_t *load_square(const char *path, int side)
{
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    if (file == NULL) return NULL;
    PngImage img;
    const int read = png_read(file, size, &img) == PNG_OK;
    free(file);
    if (!read) return NULL;
    if (img.width != (uint32_t)side || img.height != (uint32_t)side) {
        png_free(img.pixels);
        return NULL;
    }
    const size_t count = (size_t)side * (size_t)side;
    uint8_t *rgba = malloc(count * 4);
    if (rgba != NULL && img.indexed) {
        for (size_t i = 0; i < count; i++) {
            memcpy(rgba + i * 4, img.palette + img.pixels[i] * 3, 3);
            rgba[i * 4 + 3] = 255;
        }
    } else if (rgba != NULL) {
        memcpy(rgba, img.pixels, count * 4);
    }
    png_free(img.pixels);
    return rgba;
}

typedef struct {
    const char *mod, *base, *source_dir;
} Where;

/* `where` NULL: the places the game set */
static int collect(const Where *where, PortIconImage out[PORT_ICON_SIZES])
{
    int n = 0;
    for (int s = 0; s < PORT_ICON_SIZES; s++) {
        char relative[64], paths[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
        snprintf(relative, sizeof relative, "icon/spirited_%d.png", SIDES[s]);
        if (where != NULL) (void)data_path_candidates_for(where->mod, where->base, where->source_dir, relative, paths);
        else (void)data_path_candidates(relative, paths);
        uint8_t *rgba = NULL;
        for (int i = 0; rgba == NULL && i < DATA_PATH_CANDIDATES; i++)
            if (data_path_exists(paths[i])) rgba = load_square(paths[i], SIDES[s]);
        if (rgba != NULL) out[n++] = (PortIconImage){ SIDES[s], rgba };
    }
    return n;
}

int port_icon_ours_from(const char *mod, const char *base, const char *source_dir, PortIconImage out[PORT_ICON_SIZES])
{
    const Where where = { mod, base, source_dir };
    return collect(&where, out);
}

int port_icon_ours(PortIconImage out[PORT_ICON_SIZES])
{
    return collect(NULL, out);
}

void port_icon_ours_free(PortIconImage *images, int count)
{
    for (int i = 0; images != NULL && i < count; i++) {
        free(images[i].rgba);
        images[i].rgba = NULL;
    }
}

int port_icon_sets_window(void)
{
#ifdef __APPLE__
    return 0;
#else
    return 1;
#endif
}
