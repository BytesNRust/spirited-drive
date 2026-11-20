/* Sheets: see race_sheet.h. */
#include "race_sheet.h"

#include "formats/png.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FONT_W = 5, FONT_H = 7, FONT_SCALE = 2, FONT_ADVANCE = (FONT_W + 1) * FONT_SCALE, LABEL_PAD = 3 };
static const uint8_t BAND[3] = { 24, 26, 32 }, EMPTY[3] = { 38, 40, 48 }, INK[3] = { 220, 224, 232 };   /* the label band, a cell without a photo, the text */

/* 5x7 glyphs for race_sheet.c, generated: row bits, bit 4 the leftmost pixel */
static const struct { char ch; uint8_t rows[7]; } GLYPHS[] = {
    { 'A', { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
    { 'B', { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e } },
    { 'C', { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e } },
    { 'D', { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e } },
    { 'E', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f } },
    { 'F', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e } },
    { 'H', { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
    { 'I', { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e } },
    { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f } },
    { 'M', { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 } },
    { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
    { 'O', { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
    { 'P', { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 } },
    { 'Q', { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d } },
    { 'R', { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e } },
    { 'T', { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
    { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 } },
    { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11 } },
    { 'X', { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 } },
    { 'Y', { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 } },
    { 'Z', { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f } },
    { '0', { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e } },
    { '1', { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e } },
    { '2', { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f } },
    { '3', { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e } },
    { '4', { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 } },
    { '5', { 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e } },
    { '6', { 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e } },
    { '7', { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e } },
    { '9', { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e } },
    { '-', { 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00 } },
    { '_', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f } },
    { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c } },
    { ':', { 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00 } },
    { '/', { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } },
    { '(', { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } },
    { ')', { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } },
    { '+', { 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00 } },
};

static const uint8_t *glyph(char ch)
{
    const char up = (char)toupper((unsigned char)ch);
    for (size_t i = 0; i < sizeof GLYPHS / sizeof GLYPHS[0]; ++i)
        if (GLYPHS[i].ch == up) return GLYPHS[i].rows;
    return NULL;   /* a space, or a character the font does not have: a gap */
}

static void put(uint8_t *img, int w, int x, int y, const uint8_t *rgb)
{
    uint8_t *p = img + ((size_t)y * (size_t)w + (size_t)x) * 4;
    p[0] = rgb[0], p[1] = rgb[1], p[2] = rgb[2], p[3] = 255;
}

/* a text with its top left at (x, y), clipped to the rectangle x0..x1 */
static void text(uint8_t *img, int w, int x, int y, int x1, const char *s, const uint8_t *rgb)
{
    for (; *s != '\0' && x + FONT_W * FONT_SCALE <= x1; ++s, x += FONT_ADVANCE) {
        const uint8_t *rows = glyph(*s);
        for (int r = 0; rows != NULL && r < FONT_H; ++r)
            for (int c = 0; c < FONT_W; ++c)
                if (rows[r] & (1 << (FONT_W - 1 - c)))
                    for (int dy = 0; dy < FONT_SCALE; ++dy)
                        for (int dx = 0; dx < FONT_SCALE; ++dx) put(img, w, x + c * FONT_SCALE + dx, y + r * FONT_SCALE + dy, rgb);
    }
}

static void copy_str(char *to, size_t cap, const char *from)
{
    snprintf(to, cap, "%s", from != NULL ? from : "");
}

void sheet_init(RaceSheet *s, int cols)
{
    memset(s, 0, sizeof *s);
    s->cols = cols < 1 ? 1 : cols > 4 ? 4 : cols;
}

void sheet_free(RaceSheet *s)
{
    for (int i = 0; i < s->count; ++i) free(s->cell[i].rgba), s->cell[i].rgba = NULL;
    s->count = 0;
}

int sheet_add(RaceSheet *s, const char *label, const ExportCar *car, const char *project, const char *track, int night)
{
    if (s->count >= SHEET_CELLS) return -1;
    SheetCell *c = &s->cell[s->count];
    memset(c, 0, sizeof *c);
    copy_str(c->label, sizeof c->label, label);
    copy_str(c->project, sizeof c->project, project);
    copy_str(c->track, sizeof c->track, track);
    c->car = car;
    c->night = night;
    return s->count++;
}

int sheet_set_photo(RaceSheet *s, int i, const uint8_t *rgba, int w, int h)
{
    if (i < 0 || i >= s->count) return 0;
    SheetCell *c = &s->cell[i];
    const int halve = w == 2 * SHEET_CELL_W && h == 2 * SHEET_CELL_H;
    if (rgba == NULL || (!halve && !(w == SHEET_CELL_W && h == SHEET_CELL_H))) {
        char note[SHEET_NOTE];
        snprintf(note, sizeof note, "the photo is %d x %d: not 640 x 480 or 1280 x 960", w, h);
        sheet_set_failed(s, i, note);
        return 0;
    }
    uint8_t *own = malloc((size_t)SHEET_CELL_W * SHEET_CELL_H * 4);
    if (own == NULL) return sheet_set_failed(s, i, "out of memory"), 0;
    for (int y = 0; y < SHEET_CELL_H; ++y)
        for (int x = 0; x < SHEET_CELL_W; ++x) {
            uint8_t *to = own + ((size_t)y * SHEET_CELL_W + (size_t)x) * 4;
            if (!halve) {
                memcpy(to, rgba + ((size_t)y * (size_t)w + (size_t)x) * 4, 4);
                continue;
            }
            for (int k = 0; k < 3; ++k) {
                const uint8_t *q = rgba + ((size_t)(2 * y) * (size_t)w + (size_t)(2 * x)) * 4 + k;
                to[k] = (uint8_t)((q[0] + q[4] + q[(size_t)w * 4] + q[(size_t)w * 4 + 4]) / 4);
            }
            to[3] = 255;
        }
    free(c->rgba);
    c->rgba = own;
    c->state = SHEET_DONE;
    c->note[0] = '\0';
    return 1;
}

void sheet_set_failed(RaceSheet *s, int i, const char *note)
{
    if (i < 0 || i >= s->count) return;
    free(s->cell[i].rgba);
    s->cell[i].rgba = NULL;
    s->cell[i].state = SHEET_FAILED;
    copy_str(s->cell[i].note, sizeof s->cell[i].note, note);
}

int sheet_next(const RaceSheet *s)
{
    for (int i = 0; i < s->count; ++i)
        if (s->cell[i].state == SHEET_PENDING) return i;
    return -1;
}

int sheet_finished_count(const RaceSheet *s)
{
    int n = 0;
    for (int i = 0; i < s->count; ++i) n += s->cell[i].state != SHEET_PENDING;
    return n;
}

int sheet_complete(const RaceSheet *s)
{
    return s->count > 0 && sheet_finished_count(s) == s->count;
}

static void fill(uint8_t *img, int w, int x0, int y0, int cw, int ch, const uint8_t *rgb)
{
    for (int y = y0; y < y0 + ch; ++y)
        for (int x = x0; x < x0 + cw; ++x) put(img, w, x, y, rgb);
}

int sheet_compose(const RaceSheet *s, uint8_t **rgba, int *w, int *h, char *why, size_t why_cap)
{
    *rgba = NULL;
    if (s->count < 1) return snprintf(why, why_cap, "the sheet has no cells"), 0;
    const int cols = s->count < s->cols ? s->count : s->cols, rows = (s->count + s->cols - 1) / s->cols;
    const int cell_h = SHEET_CELL_H + SHEET_LABEL_H, width = cols * SHEET_CELL_W, height = rows * cell_h;
    uint8_t *img = malloc((size_t)width * (size_t)height * 4);
    if (img == NULL) return snprintf(why, why_cap, "out of memory"), 0;
    for (int i = 0; i < s->count; ++i) {
        const SheetCell *c = &s->cell[i];
        const int x0 = (i % s->cols) * SHEET_CELL_W, y0 = (i / s->cols) * cell_h;
        if (c->state == SHEET_DONE && c->rgba != NULL)
            for (int y = 0; y < SHEET_CELL_H; ++y) memcpy(img + ((size_t)(y0 + y) * (size_t)width + (size_t)x0) * 4, c->rgba + (size_t)y * SHEET_CELL_W * 4, (size_t)SHEET_CELL_W * 4);
        else {
            fill(img, width, x0, y0, SHEET_CELL_W, SHEET_CELL_H, EMPTY);
            if (c->state == SHEET_FAILED) text(img, width, x0 + 2 * FONT_ADVANCE, y0 + SHEET_CELL_H / 2, x0 + SHEET_CELL_W, "NO PHOTO", INK);
        }
        fill(img, width, x0, y0 + SHEET_CELL_H, SHEET_CELL_W, SHEET_LABEL_H, BAND);
        text(img, width, x0 + LABEL_PAD, y0 + SHEET_CELL_H + (SHEET_LABEL_H - FONT_H * FONT_SCALE) / 2, x0 + SHEET_CELL_W, c->label, INK);
    }
    // the cells a last row does not fill: the band's colour
    for (int i = s->count; i < rows * cols; ++i)
        fill(img, width, (i % s->cols) * SHEET_CELL_W, (i / s->cols) * cell_h, SHEET_CELL_W, cell_h, BAND);
    *rgba = img, *w = width, *h = height;
    return 1;
}

int sheet_write_png(const RaceSheet *s, const char *path, char *why, size_t why_cap)
{
    uint8_t *img = NULL, *png = NULL;
    int w = 0, h = 0;
    size_t len = 0;
    if (!sheet_compose(s, &img, &w, &h, why, why_cap)) return 0;
    const PngResult r = png_write_rgba(img, (uint32_t)w, (uint32_t)h, &png, &len);
    free(img);
    if (r != PNG_OK) return snprintf(why, why_cap, "cannot make the picture: %s", png_result_str(r)), 0;
    FILE *f = fopen(path, "wb");
    const int ok = f != NULL && fwrite(png, 1, len, f) == len;
    if (f != NULL && fclose(f) != 0) { png_free(png); return snprintf(why, why_cap, "cannot write %s", path), 0; }
    png_free(png);
    if (!ok) snprintf(why, why_cap, "cannot write %s", path);
    return ok;
}
