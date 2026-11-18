/* See gallery_layout.h. */
#include "game/frontend/gallery_layout.h"

#include "formats/fsh.h"
#include "formats/qfs.h"
#include "game/frontend/added_gallery.h"
#include "game/frontend/boot_quant.h"
#include "game/frontend/showcase.h"

#include <stdlib.h>
#include <string.h>

/* the rectangle x0..x1, y0..y1 (ends included, kept on the screen) of `out` mixed alpha/255 towards rgb */
static void tint(uint8_t *out, int x0, int y0, int x1, int y1, uint32_t rgb, int alpha)
{
    const int c[3] = { (int)(rgb >> 16 & 255), (int)(rgb >> 8 & 255), (int)(rgb & 255) };
    x0 = x0 < 0 ? 0 : x0, y0 = y0 < 0 ? 0 : y0, x1 = x1 >= GL_W ? GL_W - 1 : x1, y1 = y1 >= GL_H ? GL_H - 1 : y1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            uint8_t *p = out + 3 * (y * GL_W + x);
            for (int k = 0; k < 3; k++) p[k] = (uint8_t)((p[k] * (255 - alpha) + c[k] * alpha + 127) / 255);
        }
}

/* a shadow cast down and right by `d` from the box x0..x1, y0..y1: the L the box does not cover */
static void shadow(uint8_t *out, int x0, int y0, int x1, int y1, int d)
{
    tint(out, x1 + 1, y0 + d, x1 + d, y1 + d, 0, GL_SHADOW_ALPHA);
    tint(out, x0 + d, y1 + 1, x1, y1 + d, 0, GL_SHADOW_ALPHA);
}

/* a translucent panel with a one-pixel edge */
static void panel(uint8_t *out, int x0, int y0, int x1, int y1, int alpha, int edge)
{
    tint(out, x0, y0, x1, y1, GL_PANEL_RGB, alpha);
    if (!edge) return;
    tint(out, x0, y0, x1, y0, GL_PANEL_EDGE_RGB, 255), tint(out, x0, y1, x1, y1, GL_PANEL_EDGE_RGB, 255);
    tint(out, x0, y0, x0, y1, GL_PANEL_EDGE_RGB, 255), tint(out, x1, y0, x1, y1, GL_PANEL_EDGE_RGB, 255);
}

/* the photo (w x h x 3) into the picture at (x0, y0) */
static void put_photo(uint8_t *out, const uint8_t *photo, int x0, int y0, int w, int h)
{
    for (int y = 0; y < h; y++) memcpy(out + 3 * ((y0 + y) * GL_W + x0), photo + 3 * (size_t)y * w, (size_t)w * 3);
}

/* a tile's frame and shadow: black GL_FRAME px outside x0..x1, y0..y1, its shadow past that; inside the frame the background as it is (what a
 * translucent panel is mixed with, or a picture put over) */
static void framed(uint8_t *out, const uint8_t *bgnd, int x0, int y0, int w, int h)
{
    shadow(out, x0 - GL_FRAME, y0 - GL_FRAME, x0 + w - 1 + GL_FRAME, y0 + h - 1 + GL_FRAME, GL_SHADOW);
    tint(out, x0 - GL_FRAME, y0 - GL_FRAME, x0 + w - 1 + GL_FRAME, y0 + h - 1 + GL_FRAME, 0, 255);
    for (int y = y0; y < y0 + h; y++) memcpy(out + 3 * (y * GL_W + x0), bgnd + 3 * (y * GL_W + x0), (size_t)w * 3);
}

/* rows y0 .. y1 - 1 across the whole width blurred by a box of 2 * r + 1 (the edges clamped to the strip), `passes` times: horizontally, then vertically */
static void blur_strip(uint8_t *out, int y0, int y1, int r, int passes)
{
    const int h = y1 - y0;
    uint8_t *tmp = malloc((size_t)GL_W * h * 3);
    if (tmp == NULL) return;
    for (int pass = 0; pass < passes; pass++) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < GL_W; x++)
                for (int c = 0; c < 3; c++) {
                    int sum = 0;
                    for (int k = -r; k <= r; k++) sum += out[3 * ((y0 + y) * GL_W + (x + k < 0 ? 0 : x + k >= GL_W ? GL_W - 1 : x + k)) + c];
                    tmp[3 * (y * GL_W + x) + c] = (uint8_t)(sum / (2 * r + 1));
                }
        for (int y = 0; y < h; y++)
            for (int x = 0; x < GL_W; x++)
                for (int c = 0; c < 3; c++) {
                    int sum = 0;
                    for (int k = -r; k <= r; k++) sum += tmp[3 * ((y + k < 0 ? 0 : y + k >= h ? h - 1 : y + k) * GL_W + x) + c];
                    out[3 * ((y0 + y) * GL_W + x) + c] = (uint8_t)(sum / (2 * r + 1));
                }
    }
    free(tmp);
}

void gallery_compose(const uint8_t *bgnd, const uint8_t *big, const uint8_t *small, uint8_t *out)
{
    memcpy(out, bgnd, (size_t)GL_W * GL_H * 3);
    for (int y = 0; y < GL_BAND_H; y++)   /* the owner's v2: the band's texture mirrored, so it blends in */
        for (int x = 0; x < GL_BAND_W; x++) memcpy(out + 3 * (y * GL_W + x), bgnd + 3 * (y * GL_W + GL_BAND_FROM_X + GL_BAND_W - 1 - x), 3);
    blur_strip(out, 0, GL_BAND_H, GL_BLUR_R, 2);   /* port (owner 2026-11-18): the whole strip over the title line blurred, no relief left */
    /* the CD pages' order, the first drawn the farthest: Color, General, Performance, Video, History */
    framed(out, bgnd, GL_COL_X, GL_COL_Y, GL_COL_W, GL_COL_H);
    tint(out, GL_COL_X, GL_COL_Y, GL_COL_X + GL_COL_W - 1, GL_COL_Y + GL_COL_H - 1, GL_SKIN_RGB, 255);
    framed(out, bgnd, GL_GEN_X, GL_GEN_Y, GL_GEN_W, GL_GEN_H);
    panel(out, GL_GEN_X, GL_GEN_Y, GL_GEN_X + GL_GEN_W - 1, GL_GEN_Y + GL_GEN_H - 1, GL_FIGURES_ALPHA, 0);
    framed(out, bgnd, GL_PERF_X, GL_PERF_Y, GL_PERF_W, GL_PERF_H);
    put_photo(out, big, GL_PERF_X, GL_PERF_Y, GL_PERF_W, GL_PERF_H);
    framed(out, bgnd, GL_VID_X, GL_VID_Y, GL_VID_W, GL_VID_H);
    put_photo(out, small, GL_VID_X, GL_VID_Y, GL_VID_W, GL_VID_H);
    framed(out, bgnd, GL_HIST_X, GL_HIST_Y, GL_HIST_W, GL_HIST_H);
    tint(out, GL_HIST_X, GL_HIST_Y, GL_HIST_X + GL_HIST_W - 1, GL_HIST_Y + GL_HIST_H - 1, GL_HISTORY_RGB, GL_HISTORY_ALPHA);
}

static int in_rect(int x, int y, int x0, int y0, int w, int h) { return x >= x0 && x < x0 + w && y >= y0 && y < y0 + h; }

/* the two photos' boxes: the entries of the first palette part are theirs */
static int in_picture(int x, int y)
{
    return in_rect(x, y, GL_PERF_X, GL_PERF_Y, GL_PERF_W, GL_PERF_H) || in_rect(x, y, GL_VID_X, GL_VID_Y, GL_VID_W, GL_VID_H);
}

int gallery_quantise(const uint8_t *rgb, int colours, uint8_t palette[768], uint8_t *pixels)
{
    enum { PICTURE = GL_PERF_W * GL_PERF_H + GL_VID_W * GL_VID_H, REST = GL_W * GL_H - PICTURE };
    uint8_t *pic = malloc((size_t)PICTURE * 3), *rest = malloc((size_t)REST * 3);
    uint8_t *pic_i = malloc(PICTURE), *rest_i = malloc(REST);
    const int ok = pic != NULL && rest != NULL && pic_i != NULL && rest_i != NULL && colours > GL_PICTURE_COLOURS && colours <= 256;
    memset(pixels, 0, (size_t)GL_W * GL_H);
    if (ok) {
        int np = 0, nr = 0;
        for (int y = 0; y < GL_H; y++)
            for (int x = 0; x < GL_W; x++) {
                const uint8_t *c = rgb + 3 * (y * GL_W + x);
                if (in_picture(x, y)) memcpy(pic + 3 * np++, c, 3);
                else memcpy(rest + 3 * nr++, c, 3);
            }
        uint8_t rest_palette[768];
        boot_quant(pic, PICTURE, GL_PICTURE_COLOURS, palette, pic_i);
        boot_quant(rest, REST, colours - GL_PICTURE_COLOURS, rest_palette, rest_i);
        memcpy(palette + 3 * GL_PICTURE_COLOURS, rest_palette, (size_t)(colours - GL_PICTURE_COLOURS) * 3);
        np = nr = 0;
        for (int y = 0; y < GL_H; y++)
            for (int x = 0; x < GL_W; x++)
                pixels[y * GL_W + x] = in_picture(x, y) ? pic_i[np++] : (uint8_t)(GL_PICTURE_COLOURS + rest_i[nr++]);
    }
    free(pic), free(rest), free(pic_i), free(rest_i);
    return ok;
}

void gallery_lean(const uint8_t *mask, int w, int h, int num, int den, uint8_t *out, int ow)
{
    memset(out, 0, (size_t)ow * h);
    for (int y = 0; y < h; y++) {
        const int shift = (h - 1 - y) * num / den;
        for (int x = 0; x < w && x + shift < ow; x++) out[y * ow + x + shift] = mask[y * w + x];
    }
}

static int in_box(int x, int y, int x0, int x1) { return x0 <= x && x <= x1 && SHOWCASE_BOTTOM_TOP <= y && y <= SHOWCASE_BOTTOM_BOTTOM; }

/* a tile's left or right zone `zone` wide: -1, 1; 0 between */
static int zone_step(int x, int x0, int w, int zone) { return x < x0 + zone ? -1 : x >= x0 + w - zone ? 1 : 0; }

int gallery_hit(int x, int y, int *step)
{
    *step = 0;
    if (in_rect(x, y, GL_COL_X, GL_COL_Y, GL_COL_W, GL_COL_H)) return GALLERY_SETUP;   /* the SETUP tile */
    if (in_rect(x, y, GL_PERF_X, GL_PERF_Y, GL_PERF_W, GL_PERF_H)) {
        *step = zone_step(x, GL_PERF_X, GL_PERF_W, GL_ARROW_ZONE);
        return GALLERY_PHOTOS;
    }
    if (in_rect(x, y, GL_VID_X, GL_VID_Y, GL_VID_W, GL_VID_H)) return GALLERY_RECORDS;   /* Video's tile is RECORDS (owner 2026-11-18) */
    if (in_rect(x, y, GL_HIST_X, GL_HIST_Y, GL_HIST_W, GL_HIST_H)) return GALLERY_HISTORY;
    if (in_box(x, y, SHOWCASE_PREVIOUS_LEFT, SHOWCASE_PREVIOUS_RIGHT)) return GALLERY_PREVIOUS;
    if (in_box(x, y, SHOWCASE_NEXT_LEFT, SHOWCASE_NEXT_RIGHT)) return GALLERY_NEXT;
    if (in_box(x, y, SHOWCASE_DONE_LEFT, SHOWCASE_DONE_RIGHT)) return GALLERY_DONE;
    return -1;
}

/* the SETUP panel (open over the tile and General): its row's number and the side of the value a click at (x, y) is on, -1 for nothing */
int gallery_setup_hit(int x, int y, int *step)
{
    *step = 0;
    if (!in_rect(x, y, GL_SETUP_X, GL_SETUP_Y, GL_SETUP_W, GL_SETUP_H)) return -1;
    const int row = (y - GL_SETUP_Y - GL_SETUP_ROW0 + GL_SETUP_STEP / 2) / GL_SETUP_STEP;
    if (y < GL_SETUP_Y + GL_SETUP_ROW0 - GL_SETUP_STEP / 2 || row < 0 || row >= GALLERY_SETUP_ROWS) return -1;
    if (row >= SETUP_TRANSMISSION) *step = 1;   /* a switch, as in Options: a click anywhere on its row changes it */
    else if (x >= GL_SETUP_X + GL_SETUP_VALUE_X - 16) *step = x < GL_SETUP_X + GL_SETUP_VALUE_X + GL_SETUP_VALUE_W / 2 ? -1 : 1;   /* the label's side: the row alone */
    return row;
}

/* --- the bottom row cut from the CD's art --- */

int gallery_row_is_face(const uint8_t rgb[3])
{
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    return r >= 150 && g >= 130 && r - g < 70 && r >= b;
}

void gallery_row_clear(GalleryRow *row) { memset(row, 0, sizeof *row); }

/* the colour's number in the row's (1..), added when there is room, else the nearest kept */
static int row_colour(GalleryRow *row, const uint8_t *c)
{
    int best = 0, best_d = 1 << 30;
    for (int k = 0; k < row->colours; k++) {
        const int dr = c[0] - row->rgb[k][0], dg = c[1] - row->rgb[k][1], db = c[2] - row->rgb[k][2], d = dr * dr + dg * dg + db * db;
        if (d == 0) return k + 1;
        if (d < best_d) best = k, best_d = d;
    }
    if (row->colours < GL_ROW_COLOURS) {
        memcpy(row->rgb[row->colours], c, 3);
        return ++row->colours;
    }
    return best + 1;
}

void gallery_row_put(GalleryRow *row, int look, const uint8_t *pixels, int w, int h, int x0, int y0, const uint8_t pal[768])
{
    if (look < 0 || look >= GL_ROW_LOOKS) return;
    for (int y = 0; y < h; y++) {
        const int ry = y0 + y - GL_ROW_Y;
        if (ry < 0 || ry >= GL_ROW_H) continue;
        for (int x = 0; x < w; x++) {
            const int rx = x0 + x;
            if (rx < 0 || rx >= GL_ROW_W) continue;
            const uint8_t *c = pal + 3 * pixels[(size_t)y * w + x];
            uint8_t *f = &row->face[look][ry * GL_ROW_W + rx];
            *f = gallery_row_is_face(c) ? (uint8_t)row_colour(row, c) : 0;
        }
    }
}

/* the bank's shape `tag` into the look, at its own place; 0 when it has none */
static int put_shape(GalleryRow *row, int look, const Fsh *fsh, const char tag[4], const uint8_t pal[768])
{
    uint32_t index;
    FshEntry e;
    if (fsh_find(fsh, tag, &index) != FSH_OK || fsh_entry(fsh, index, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP ||
        e.payload_size < (size_t)e.width * e.height)
        return 0;
    gallery_row_put(row, look, e.payload, e.width, e.height, e.misc[2], e.misc[3], pal);
    return 1;
}

int gallery_row_cut(GalleryRow *row, const uint8_t *file, size_t size)
{
    gallery_row_clear(row);
    uint8_t *data = NULL;
    size_t data_size = size;
    if (qfs_is_compressed(file, size)) {
        if (qfs_decompressed_size(file, size, &data_size) != QFS_OK || (data = malloc(data_size)) == NULL ||
            qfs_decompress(file, size, data, data_size) != QFS_OK) {
            free(data);
            return 0;
        }
    }
    Fsh fsh;
    uint32_t index;
    FshEntry e;
    uint8_t pal[768];
    int ok = fsh_open(&fsh, data != NULL ? data : file, data_size) == FSH_OK && fsh_find(&fsh, "!pal", &index) == FSH_OK &&
             fsh_entry(&fsh, index, &e) == FSH_OK && fsh_palette_rgb(&e, pal) == FSH_OK;
    static const char *const LIT[3] = { "donl", "prel", "nexl" };
    for (int look = 0; ok && look < GL_ROW_LOOKS; look++) {   /* the background has them unlit, as the CD's page first shows them */
        ok = put_shape(row, look, &fsh, "bgnd", pal);
        if (ok && look > 0) ok = put_shape(row, look, &fsh, LIT[look - 1], pal);
    }
    free(data);
    if (!ok) {
        gallery_row_clear(row);
        return 0;
    }
    for (int look = 0; look < GL_ROW_LOOKS; look++)
        for (int i = 0; i < GL_ROW_H * GL_ROW_W; i++)
            if (row->face[look][i] != 0) row->count[row->face[look][i] - 1]++;
    return 1;
}

int gallery_row_look(int item)
{
    return item == GALLERY_DONE ? GL_ROW_DONE : item == GALLERY_PREVIOUS ? GL_ROW_PREVIOUS : item == GALLERY_NEXT ? GL_ROW_NEXT : GL_ROW_PLAIN;
}

void gallery_row_draw(const GalleryRow *row, int look, uint8_t *to, int tw, int th, const uint8_t map[GL_ROW_COLOURS], int shadow)
{
    if (look < 0 || look >= GL_ROW_LOOKS) return;
    const uint8_t *face = row->face[look];
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < GL_ROW_H; y++)
            for (int x = 0; x < GL_ROW_W; x++) {
                const int k = face[y * GL_ROW_W + x], d = pass == 0 ? GL_ROW_SHADOW : 0, ty = GL_ROW_Y + y + d, tx = x + d;
                if (k == 0 || tx >= tw || ty >= th) continue;
                to[ty * tw + tx] = pass == 0 ? (uint8_t)shadow : map[k - 1];
            }
}
