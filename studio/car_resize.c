/* tnfs-studio car resize (studio/car_resize.h). */
#include "studio/car_resize.h"

#include "formats/car_piece.h"
#include "formats/car_resize.h"
#include "formats/car_texture.h"
#include "formats/cfm_build.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "studio/car_import.h"
#include "studio/fsio.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USAGE "usage: tnfs-studio car resize PIECE_DIR NAME WxH [--image FILE.png] [--low] | tnfs-studio car resize PIECE_DIR NAME --image FILE.png [--low]"

enum { SKINS_CAP = CAR_PIECE_SKINS_MAX + 1, BANK_CAP = 256 };

typedef struct {
    const char *piece, *name, *size, *image;
    int low;
} Args;

/* a skin's picture scaled, ready to write */
typedef struct {
    char path[PATH_MAX];
    uint8_t *png;
    size_t len;
} Skin;

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--image") == 0 && i + 1 < argc) a->image = argv[++i];
        else if (strcmp(argv[i], "--low") == 0) a->low = 1;
        else if (argv[i][0] != '-' && a->piece == NULL) a->piece = argv[i];
        else if (argv[i][0] != '-' && a->name == NULL) a->name = argv[i];
        else if (argv[i][0] != '-' && a->size == NULL) a->size = argv[i];
        else { exchange_fail(err, "unexpected argument '%s'; %s", argv[i], USAGE); return 0; }
    }
    if (a->piece == NULL || a->name == NULL || (a->size == NULL && a->image == NULL)) { exchange_fail(err, USAGE); return 0; }
    if (strlen(a->name) != 4) { exchange_fail(err, "'%s' is not a bank entry's name (4 characters: rsid, topv, tyr1 ...)", a->name); return 0; }
    return 1;
}

/* "WxH" */
static int side_pair(const char *text, uint16_t *w, uint16_t *h, ExchangeError *err)
{
    char *end;
    const long lw = strtol(text, &end, 10);
    const long lh = *end == 'x' || *end == 'X' ? strtol(end + 1, &end, 10) : 0;
    if (*end != '\0' || lw < 1 || lh < 1 || lw > SHPI_MAX_SIDE || lh > SHPI_MAX_SIDE) {
        exchange_fail(err, "'%s' is not a size WxH of 1 to %d a side (e.g. 536x216)", text, SHPI_MAX_SIDE);
        return 0;
    }
    *w = (uint16_t)lw, *h = (uint16_t)lh;
    return 1;
}

/* the level's bank's palette (its first, as car import fits a picture to it) */
static int bank_palette(const uint8_t *cfm, size_t len, int level, uint8_t pal[SHPI_PALETTE_BYTES], uint8_t *type, ExchangeError *err)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t clen[CFM_CHILDREN], n = 0;
    ShpiDescEntry *e = malloc(BANK_CAP * sizeof *e);
    int ok = e != NULL && cfm_describe(cfm, len, child, clen, err) && shpi_describe(child[2 * level + 1], clen[2 * level + 1], e, BANK_CAP, &n, err);
    size_t i = 0;
    while (ok && i < n && e[i].type != 0x22 && e[i].type != 0x24) i++;
    if (ok && i == n) ok = 0, exchange_fail(err, "the %s bank has no palette", level ? "far model's" : "high model's");
    if (ok) memcpy(pal, e[i].data, SHPI_PALETTE_BYTES), *type = e[i].type;
    if (e == NULL) exchange_fail(err, "out of memory");
    free(e);
    return ok;
}

/* the picture `path` as w x h pixels of the bank's palette (malloc'd) */
static uint8_t *image_pixels(const char *path, const uint8_t *cfm, size_t len, int level, uint16_t *w, uint16_t *h, ExchangeError *err)
{
    uint8_t pal[SHPI_PALETTE_BYTES], type = 0, *pixels = NULL;
    size_t size = 0;
    uint8_t *png = bank_palette(cfm, len, level, pal, &type, err) ? fsio_read_file(path, &size, err) : NULL;
    if (png == NULL) return NULL;
    CarTextureStats st;
    if (!car_texture_from_png(png, size, pal, type, &pixels, w, h, &st, err)) {
        char why[sizeof err->message];
        snprintf(why, sizeof why, "%s", err->message);
        exchange_fail(err, "%s: %s", path, why);
    } else if (st.moved > 0) {
        printf("car resize: %s: %u pixels of colours the bank's palette does not hold, given the nearest\n", path, st.moved);
    }
    free(png);
    return pixels;
}

/* a skin's picture (indexed or not, as it is) at w x h (malloc'd PNG) */
static uint8_t *scaled_png(const uint8_t *png, size_t size, uint16_t w, uint16_t h, size_t *out_len, const char *path, ExchangeError *err)
{
    PngImage img;
    const PngResult r = png_read(png, size, &img);
    if (r != PNG_OK) { exchange_fail(err, "%s: %s", path, png_result_str(r)); return NULL; }
    const size_t bpp = img.indexed ? 1 : 4;
    uint8_t *px = malloc((size_t)w * h * bpp), *out = NULL;
    for (uint32_t y = 0; px != NULL && y < h; y++)
        for (uint32_t x = 0; x < w; x++)
            memcpy(px + ((size_t)y * w + x) * bpp, img.pixels + ((size_t)(y * img.height / h) * img.width + x * img.width / w) * bpp, bpp);
    const PngResult wr = px == NULL ? PNG_ERR_MEMORY
                         : img.indexed ? png_write_indexed(px, w, h, img.palette, &out, out_len)
                                       : png_write_rgba(px, w, h, &out, out_len);
    if (wr != PNG_OK) exchange_fail(err, "%s: %s", path, png_result_str(wr)), out = NULL;
    png_free(img.pixels);
    free(px);
    return out;
}

/* the piece's skins of the texture (skins/<skin>/NAME.png, or NAME_lo.png), scaled; how many in `*n`; 0 when one cannot be */
static int scaled_skins(const Args *a, uint16_t w, uint16_t h, Skin *skins, int *n, ExchangeError *err)
{
    char dir[PATH_MAX], file[16];
    *n = 0;
    if (!fsio_find_ci(a->piece, "skins", dir, sizeof dir)) return 1;
    snprintf(file, sizeof file, "%s%s.png", a->name, a->low ? "_lo" : "");
    DIR *d = opendir(dir);
    if (d == NULL) return 1;
    int ok = 1;
    for (struct dirent *e; ok && (e = readdir(d)) != NULL;) {
        if (e->d_name[0] == '.') continue;
        char skin[PATH_MAX], path[PATH_MAX];
        snprintf(skin, sizeof skin, "%s/%s", dir, e->d_name);
        if (!fsio_find_ci(skin, file, path, sizeof path)) continue;
        if (*n == SKINS_CAP) { exchange_fail(err, "%s: more skins than a piece holds", dir); ok = 0; break; }
        size_t size = 0;
        uint8_t *png = fsio_read_file(path, &size, err);
        Skin *s = &skins[*n];
        ok = png != NULL && (s->png = scaled_png(png, size, w, h, &s->len, path, err)) != NULL;
        if (ok) snprintf(s->path, sizeof s->path, "%s", path), (*n)++;
        free(png);
    }
    closedir(d);
    return ok;
}

static int resize(const Args *a, ExchangeError *err)
{
    ModInfo info;
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], cfm_path[PATH_MAX];
    if (!car_piece_card(a->piece, &info, err)) return 0;
    car_piece_paths(info.stem, paths);
    if (!fsio_find_ci(a->piece, paths[0], cfm_path, sizeof cfm_path)) { exchange_fail(err, "%s has no %s", a->piece, paths[0]); return 0; }
    uint16_t w = 0, h = 0, iw = 0, ih = 0;
    if (a->size != NULL && !side_pair(a->size, &w, &h, err)) return 0;
    size_t len = 0, out_len = 0;
    uint8_t *cfm = fsio_read_file(cfm_path, &len, err), *pixels = NULL, *out = NULL;
    const int level = a->low ? 1 : 0;
    int ok = cfm != NULL;
    if (ok && a->image != NULL) {
        ok = (pixels = image_pixels(a->image, cfm, len, level, &iw, &ih, err)) != NULL;
        if (ok && a->size != NULL && (iw != w || ih != h))
            ok = 0, exchange_fail(err, "%s is %ux%u, not the %ux%u asked", a->image, iw, ih, w, h);
        if (ok) w = iw, h = ih;
    }
    CarResizeStats st;
    Skin skins[SKINS_CAP];
    int n_skins = 0;
    memset(skins, 0, sizeof skins);
    ok = ok && car_resize(cfm, len, level, a->name, w, h, pixels, &out, &out_len, &st, err) && scaled_skins(a, w, h, skins, &n_skins, err);
    ok = ok && fsio_write_atomic(cfm_path, out, out_len, err);
    for (int i = 0; ok && i < n_skins; i++) ok = fsio_write_atomic(skins[i].path, skins[i].png, skins[i].len, err);
    if (ok) {
        printf("car resize: %s: %.4s %ux%u -> %ux%u%s; %u faces, %u uvs scaled (%u split from another texture's)\n", cfm_path, a->name,
               st.was_w, st.was_h, w, h, a->low ? " (far model)" : "", st.faces, st.uvs_scaled, st.uvs_split);
        if (n_skins > 0) printf("car resize: %d skin pictures scaled with it\n", n_skins);
        if (st.faces == 0) printf("car resize: no face of the %s model has uvs on %.4s\n", a->low ? "far" : "high", a->name);
    }
    for (int i = 0; i < n_skins; i++) free(skins[i].png);
    free(cfm), free(pixels), free(out);
    return ok;
}

int car_resize_command(int argc, char **argv, ExchangeError *err)
{
    Args a;
    return parse(argc, argv, &a, err) && resize(&a, err);
}
