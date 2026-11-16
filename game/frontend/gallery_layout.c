/* See gallery_layout.h. */
#include "game/frontend/gallery_layout.h"

#include "game/frontend/added_gallery.h"
#include "game/frontend/boot_quant.h"

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

void gallery_next_place(int text_w, int *left_x, int *text_x, int *right_x)
{
    const int whole = GL_ARROW_W + GL_NEXT_GAP + text_w + GL_NEXT_GAP + GL_ARROW_W;
    *left_x = (GL_W - whole) / 2;
    *text_x = *left_x + GL_ARROW_W + GL_NEXT_GAP;
    *right_x = *text_x + text_w + GL_NEXT_GAP;
}

/* a box of the right column at y: its item, and which arrow's half (the arrow and the room by it) */
static int box_hit(int x, int y, int box_y, int item, int *step)
{
    if (x < GL_COLUMN_X || x >= GL_COLUMN_X + GL_COLUMN_W || y < box_y || y >= box_y + GL_BOX_H) return -1;
    const int zone = GL_ARROW_IN + GL_ARROW_W + 8;
    *step = x < GL_COLUMN_X + zone ? -1 : x >= GL_COLUMN_X + GL_COLUMN_W - zone ? 1 : 0;
    return item;
}

int gallery_hit(int x, int y, int next_text_w, int done_w, int *step)
{
    *step = 0;
    int item = box_hit(x, y, GL_SKIN_Y, GALLERY_SKIN, step);
    if (item < 0) item = box_hit(x, y, GL_COLOUR_Y, GALLERY_COLOUR, step);
    if (item >= 0 || y < GL_BOTTOM_Y - 6 || y >= GL_BOTTOM_Y + GL_BOTTOM_H) return item;
    *step = 0;
    int l, t, r;
    gallery_next_place(next_text_w, &l, &t, &r);
    if (x >= GL_DONE_X - 4 && x < GL_DONE_X + done_w + 4) return GALLERY_DONE;
    if (x >= l - 6 && x < t - GL_NEXT_GAP / 2) return GALLERY_PREVIOUS;
    if (x >= t - GL_NEXT_GAP / 2 && x < r + GL_ARROW_W + 6) return GALLERY_NEXT;
    return -1;
}
