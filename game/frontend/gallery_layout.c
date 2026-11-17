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

void gallery_compose(const uint8_t *bgnd, const uint8_t *picture, uint8_t *out)
{
    memcpy(out, bgnd, (size_t)GL_W * GL_H * 3);
    for (int y = 0; y < GL_BAND_H; y++)   /* the owner's v2: the band's texture mirrored, so it blends in */
        for (int x = 0; x < GL_BAND_W; x++) memcpy(out + 3 * (y * GL_W + x), bgnd + 3 * (y * GL_W + GL_BAND_FROM_X + GL_BAND_W - 1 - x), 3);
    const int px1 = GL_PICTURE_X + GL_PICTURE_W - 1, py1 = GL_PICTURE_Y + GL_PICTURE_H - 1;
    shadow(out, GL_PICTURE_X - GL_FRAME, GL_PICTURE_Y - GL_FRAME, px1 + GL_FRAME, py1 + GL_FRAME, GL_SHADOW);
    tint(out, GL_PICTURE_X - GL_FRAME, GL_PICTURE_Y - GL_FRAME, px1 + GL_FRAME, py1 + GL_FRAME, 0, 255);
    for (int y = 0; y < GL_PICTURE_H; y++)
        memcpy(out + 3 * ((GL_PICTURE_Y + y) * GL_W + GL_PICTURE_X), picture + 3 * y * GL_PICTURE_W, (size_t)GL_PICTURE_W * 3);
    const int cx1 = GL_COLUMN_X + GL_COLUMN_W - 1;
    panel(out, GL_COLUMN_X, GL_FIGURES_Y, cx1, GL_FIGURES_Y + GL_FIGURES_H - 1, GL_FIGURES_ALPHA, 1);
    shadow(out, GL_COLUMN_X, GL_SKIN_Y, cx1, GL_SKIN_Y + GL_BOX_H - 1, GL_SHADOW);   /* the picture's depth: one light for the page */
    shadow(out, GL_COLUMN_X, GL_COLOUR_Y, cx1, GL_COLOUR_Y + GL_BOX_H - 1, GL_SHADOW);   /* the picture's depth: one light for the page */
    tint(out, GL_COLUMN_X, GL_SKIN_Y, cx1, GL_SKIN_Y + GL_BOX_H - 1, GL_SKIN_RGB, 255);   /* Color's the paint's when one is picked */
    tint(out, GL_COLUMN_X, GL_COLOUR_Y, cx1, GL_COLOUR_Y + GL_BOX_H - 1, GL_SKIN_RGB, 255);
    panel(out, GL_FOOT_X0, GL_FOOT_Y0, GL_FOOT_X1, GL_FOOT_Y1, GL_FOOT_ALPHA, 0);
}

static int in_picture(int x, int y)
{
    return x >= GL_PICTURE_X && x < GL_PICTURE_X + GL_PICTURE_W && y >= GL_PICTURE_Y && y < GL_PICTURE_Y + GL_PICTURE_H;
}

int gallery_quantise(const uint8_t *rgb, int colours, uint8_t palette[768], uint8_t *pixels)
{
    enum { PICTURE = GL_PICTURE_W * GL_PICTURE_H, REST = GL_W * GL_H - PICTURE };
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

/* a box of the right column at y: its item, and which arrow's half (the arrow and the room by it) */
static int box_hit(int x, int y, int box_y, int item, int *step)
{
    if (x < GL_COLUMN_X || x >= GL_COLUMN_X + GL_COLUMN_W || y < box_y || y >= box_y + GL_BOX_H) return -1;
    const int zone = GL_ARROW_IN + GL_ARROW_W + 8;
    *step = x < GL_COLUMN_X + zone ? -1 : x >= GL_COLUMN_X + GL_COLUMN_W - zone ? 1 : 0;
    return item;
}

static int in_box(int x, int y, int x0, int x1) { return x0 <= x && x <= x1 && SHOWCASE_BOTTOM_TOP <= y && y <= SHOWCASE_BOTTOM_BOTTOM; }

int gallery_hit(int x, int y, int *step)
{
    *step = 0;
    int item = box_hit(x, y, GL_SKIN_Y, GALLERY_SKIN, step);
    if (item < 0) item = box_hit(x, y, GL_COLOUR_Y, GALLERY_COLOUR, step);
    if (item >= 0) return item;
    *step = 0;
    if (in_box(x, y, SHOWCASE_PREVIOUS_LEFT, SHOWCASE_PREVIOUS_RIGHT)) return GALLERY_PREVIOUS;
    if (in_box(x, y, SHOWCASE_NEXT_LEFT, SHOWCASE_NEXT_RIGHT)) return GALLERY_NEXT;
    if (in_box(x, y, SHOWCASE_DONE_LEFT, SHOWCASE_DONE_RIGHT)) return GALLERY_DONE;
    return -1;
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
