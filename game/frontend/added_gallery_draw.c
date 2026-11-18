/* See added_gallery_page.h: an added car's gallery page, what is drawn over its still part (dep59 showcase, PR B): the name, the tiles' labels,
 * the focus frame, the big photo's arrows and count, General's figures, History's text, the Color tile's two rows. */
#include "game/frontend/added_gallery_page.h"

#include "game/sys/text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *to;
    int w, h;
} Target;

/* x0..x1, y0..y1 (ends included) in entry `index` */
static void fill(const Target *t, int x0, int y0, int x1, int y1, int index)
{
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < t->h; y++)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < t->w; x++) t->to[y * t->w + x] = (uint8_t)index;
}

static void edge(const Target *t, int x0, int y0, int x1, int y1, int width, int index)
{
    fill(t, x0, y0, x1, y0 + width - 1, index), fill(t, x0, y1 - width + 1, x1, y1, index);
    fill(t, x0, y0, x0 + width - 1, y1, index), fill(t, x1 - width + 1, y0, x1, y1, index);
}

/* an arrow GL_ARROW_W x GL_ARROW_H pointing left or right, its top left at (x, y), over a black one a pixel down and right */
static void arrow(const Target *t, int x, int y, int left, int index)
{
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i <= GL_ARROW_H; i++) {
            const int half = i <= GL_ARROW_H / 2 ? i : GL_ARROW_H - i, span = half * GL_ARROW_W / (GL_ARROW_H / 2);
            const int d = pass == 0 ? 1 : 0, x0 = left ? x + GL_ARROW_W - span : x;
            fill(t, x0 + d, y + i + d, x0 + span + d, y + i + d, pass == 0 ? BLACK : index);
        }
}

/* the name big and leant, white, with the original titles' shadow; smaller when it would not fit the screen */
static void name(const Page *p)
{
    char said[sizeof p->card.info.name];
    text_words(p->card.info.name, said, sizeof said);   /* the main face: its ',' is a comma */
    static const int SIZES[3][2] = { { GL_NAME_NUM, GL_NAME_DEN }, { 3, 2 }, { 1, 1 } };
    for (int k = 0; k < 3; k++) {
        const int num = SIZES[k][0], den = SIZES[k][1], w = text_width_scaled(said, num, den) + 2, h = text_mask_rows(said, num, den);
        const int ow = w + h * LEAN_NUM / LEAN_DEN + 2;
        if (h <= 0 || (GL_NAME_X + ow > GL_W - GL_NAME_X && k < 2)) continue;
        uint8_t *mask = malloc((size_t)w * h), *leant = malloc((size_t)ow * h);
        if (mask != NULL && leant != NULL && text_mask_scaled(said, num, den, mask, w, h)) {
            gallery_lean(mask, w, h, LEAN_NUM, LEAN_DEN, leant, ow);
            text_select(TEXT_WHITE);
            int top, bottom;
            text_ink_scaled(said, num, den, &top, &bottom);
            text_draw_mask(leant, ow, h, GL_NAME_X, (GL_BAND_H - (bottom - top)) / 2 - top);   /* owner 2026-11-18: centred in the strip */
        }
        free(mask), free(leant);
        return;
    }
}

void added_gallery_draw_title(const Page *p)
{
    name(p);
    text_select(TEXT_RED);
    text_draw_smooth("Car Showcase", GL_HEADING_X, GL_HEADING_Y, GL_HEADING_NUM, GL_HEADING_DEN);
}

/* `text` in the page's main face at 3/4, solid `index` with a one-pixel shadow `shadow` down and right, its pen at (x, y) */
static void shaded(const Target *t, const char *text, int x, int y, int index, int shadow)
{
    const int w = text_width_scaled(text, 3, 4) + 2, h = text_mask_rows(text, 3, 4);
    uint8_t *mask = h > 0 ? malloc((size_t)w * h) : NULL;
    if (mask != NULL && text_mask_scaled(text, 3, 4, mask, w, h))
        for (int pass = 0; pass < 2; pass++)
            for (int j = 0; j < h; j++)
                for (int i = 0; i < w; i++) {
                    const int px = x + i + (pass == 0), py = y + j + (pass == 0);
                    if (mask[j * w + i] >= 6 && px >= 0 && px < t->w && py >= 0 && py < t->h) t->to[py * t->w + px] = (uint8_t)(pass == 0 ? shadow : index);
                }
    free(mask);
}

/* a tile's label at its foot, as the CD pages have it (owner 2026-11-18): black, its shadow cream, yellow when the tile is the item */
static void label(const Target *t, const char *text, int x, int y, int h, int lit)
{
    shaded(t, text, x + 8, y + h - 24, BLACK, lit ? YELLOW : CREAM);
}

/* `text` cut (with a "." at its end) until the small face shows it in `width` pixels */
static void fitted(const char *text, int width, char *out, int size)
{
    snprintf(out, (size_t)size, "%s", text);
    while (out[0] != 0 && text_small_width(out) > width) {
        const size_t n = strlen(out);
        out[n - 1] = 0;
        if (n > 1) out[n - 2] = '.';
    }
}

/* a row's label at the left of x0..x1 and its value at the right, in the small face */
static void row(const char *name_, const char *value, int x0, int x1, int y, int value_style)
{
    char said[64], fits[64];
    text_select(TEXT_DIM);
    text_small_draw(name_, x0, y);
    if (text_small_words(value, said, sizeof said) == 0) snprintf(said, sizeof said, "-");
    fitted(said, x1 - x0 - text_small_width(name_) - 8, fits, sizeof fits);
    text_select((TextStyle)value_style);
    text_small_draw(fits, x1 - text_small_width(fits), y);
}

static void general(const Page *p)
{
    const char *const LABEL[4] = { "Year", "Power", "Weight", "Category" };
    const char *const value[4] = { p->card.info.year, p->card.info.power, p->card.info.weight, p->card.info.category };
    for (int i = 0; i < 4; i++) row(LABEL[i], value[i], GL_GEN_X + 8, GL_GEN_X + GL_GEN_W - 8, GL_GEN_Y + 6 + i * 20, TEXT_CREAM);
}

int added_gallery_history_visible(void)
{
    return (GL_HIST_H - 8 - 28) / text_small_line_height();
}

static void history(const Page *p, const Target *t)
{
    const int x = GL_HIST_X + 8, visible = added_gallery_history_visible(), lh = text_small_line_height();
    text_select(TEXT_WHITE);
    for (int i = 0; i < visible && p->state.scroll + i < p->line_count; i++) {
        const HistoryLine *l = &p->lines[p->state.scroll + i];
        char line[160];
        snprintf(line, sizeof line, "%.*s", l->len, l->text);
        text_small_draw(line, x, GL_HIST_Y + 6 + i * lh);
    }
    if (p->line_count > visible) {   /* a thumb on a track at the right edge: where the text is */
        const int tx = GL_HIST_X + GL_HIST_W - 7, ty = GL_HIST_Y + 8, th = visible * lh;
        fill(t, tx, ty, tx + 2, ty + th - 1, DIM);
        const int thumb = th * visible / p->line_count, at = (th - thumb) * p->state.scroll / (p->line_count - visible);
        fill(t, tx, ty + at, tx + 2, ty + at + thumb - 1, CREAM);
    }
}

/* the SETUP tile closed: the Supra's engine in it (flat when it was not made), its word and an arrow, lit when it is the item */
static void setup_tile(const Target *t, const Page *p)
{
    const int lit = p->state.item == GALLERY_SETUP;
    if (p->tile_ok)
        for (int y = 0; y < GL_COL_H; y++)
            for (int x = 0; x < GL_COL_W; x++)
                if (GL_COL_X + x < t->w && GL_COL_Y + y < t->h) t->to[(GL_COL_Y + y) * t->w + GL_COL_X + x] = p->tile[y * GL_COL_W + x];
    shaded(t, "SETUP", GL_COL_X + 14, GL_COL_Y + 20, BLACK, lit ? YELLOW : CREAM);
    arrow(t, GL_COL_X + GL_COL_W - 24, GL_COL_Y + (GL_COL_H - GL_ARROW_H) / 2, 0, lit ? YELLOW : CREAM);
}

/* the value the panel's row `row` shows */
static void setup_value(const Page *p, int row, char *out, int size)
{
    const GalleryState *s = &p->state;
    switch (row) {
    case SETUP_COLOUR: text_small_words(s->paint == 0 ? "Default" : p->card.info.paints[s->paint - 1].name, out, size); break;
    case SETUP_SKIN: text_small_words(s->skin == 0 ? "Default" : p->card.skin[s->skin], out, size); break;
    case SETUP_TRANSMISSION: snprintf(out, (size_t)size, "%s", s->transmission == 0 ? "Automatic" : "Manual"); break;   /* Options' words */
    case SETUP_ABS: snprintf(out, (size_t)size, "%s", s->abs == SETUP_NOT_OFFERED ? "n/a" : s->abs == 0 ? "On" : "Off"); break;
    default: snprintf(out, (size_t)size, "%s", s->traction == SETUP_NOT_OFFERED ? "n/a" : s->traction == 0 ? "On" : "Off"); break;
    }
}

/* a preview box (black outline) of entry `index` at (x, y) */
static void preview(const Target *t, int x, int y, int index)
{
    fill(t, x, y, x + GL_SETUP_PREVIEW_W - 1, y + GL_SETUP_PREVIEW_H - 1, BLACK);
    fill(t, x + 1, y + 1, x + GL_SETUP_PREVIEW_W - 2, y + GL_SETUP_PREVIEW_H - 2, index);
}

/* the SETUP panel open over the tile and General's top (a modal): its ground, a border, the title in red and the rows with Options' words
 * (the focused one yellow, one that takes no key dim); Color and Skin have their arrows and a preview, the switches only their value */
static void setup_panel(const Target *t, const Page *p)
{
    static const char *const LABEL[SETUP_ROWS] = { "Color", "Skin", "Transmission", "ABS", "Traction control" };
    const GalleryState *s = &p->state;
    if (p->art != NULL && p->art->ok) {   /* the stone: its ground, and while it moves the slab where frame k's is */
        for (int y = 0; y < GL_SETUP_H; y++)
            for (int x = 0; x < GL_SETUP_W; x++) {
                const int at = y * GL_SETUP_W + x, px = GL_SETUP_X + x, py = GL_SETUP_Y + y;
                if (px >= t->w || py >= t->h) continue;
                t->to[py * t->w + px] = p->moving && p->art->stone[p->frame][at] ? p->art->slab[at] : p->art->body[at];
            }
        if (p->moving) return;   /* the words come when it is open */
    } else
        fill(t, GL_SETUP_X, GL_SETUP_Y, GL_SETUP_X + GL_SETUP_W - 1, GL_SETUP_Y + GL_SETUP_H - 1, p->panel_fill);
    edge(t, GL_SETUP_X, GL_SETUP_Y, GL_SETUP_X + GL_SETUP_W - 1, GL_SETUP_Y + GL_SETUP_H - 1, 1, CREAM);
    text_select(TEXT_RED);
    text_draw_smooth("CAR SETUP", GL_SETUP_X + 12, GL_SETUP_Y + 8, GL_HEADING_NUM, GL_HEADING_DEN);   /* as "Car Showcase", red */
    for (int row = 0; row < SETUP_ROWS; row++) {
        const int y = GL_SETUP_Y + GL_SETUP_ROW0 + row * GL_SETUP_STEP, on = added_gallery_setup_row_on(&p->card, s, row), lit = s->row == row;
        const int choice = row == SETUP_COLOUR || row == SETUP_SKIN, vx = GL_SETUP_X + GL_SETUP_VALUE_X;
        char value[64], fits[64];
        setup_value(p, row, value, sizeof value);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_CREAM);
        text_small_draw(LABEL[row], GL_SETUP_X + 12, y);
        fitted(value, GL_SETUP_VALUE_W - (choice ? 2 * (GL_ARROW_W + 4) : 0), fits, sizeof fits);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_WHITE);
        text_small_draw(fits, vx + (GL_SETUP_VALUE_W - text_small_width(fits)) / 2, y);
        if (!choice || !on) continue;
        const int index = lit ? YELLOW : CREAM;
        arrow(t, vx, y + 1, 1, index);
        arrow(t, vx + GL_SETUP_VALUE_W - GL_ARROW_W, y + 1, 0, index);
        preview(t, vx + GL_SETUP_VALUE_W + 8, y - 1, row == SETUP_COLOUR ? SWATCH : SKIN_SWATCH);
    }
}

void added_gallery_draw_tiles(const Page *p, uint8_t *to, int tw, int th)
{
    const Target t = { to, tw, th };
    const GalleryState *s = &p->state;
    /* the focus: a yellow frame round the tile the item is, over the black one */
    if (s->item == GALLERY_PHOTOS)
        edge(&t, GL_PERF_X - GL_FRAME, GL_PERF_Y - GL_FRAME, GL_PERF_X + GL_PERF_W + GL_FRAME - 1, GL_PERF_Y + GL_PERF_H + GL_FRAME - 1, GL_FRAME, YELLOW);
    if (s->item == GALLERY_HISTORY)
        edge(&t, GL_HIST_X - GL_FRAME, GL_HIST_Y - GL_FRAME, GL_HIST_X + GL_HIST_W + GL_FRAME - 1, GL_HIST_Y + GL_HIST_H + GL_FRAME - 1, GL_FRAME, YELLOW);
    if (s->item == GALLERY_SETUP && !s->setup)
        edge(&t, GL_COL_X - GL_FRAME, GL_COL_Y - GL_FRAME, GL_COL_X + GL_COL_W + GL_FRAME - 1, GL_COL_Y + GL_COL_H + GL_FRAME - 1, GL_FRAME, YELLOW);
    label(&t, "GALLERY", GL_PERF_X, GL_PERF_Y, GL_PERF_H, s->item == GALLERY_PHOTOS);
    if (!s->setup) label(&t, "GENERAL", GL_GEN_X, GL_GEN_Y, GL_GEN_H, 0);   /* the open panel covers its top: its label would show cut */
    label(&t, "HISTORY", GL_HIST_X, GL_HIST_Y, GL_HIST_H, s->item == GALLERY_HISTORY);   /* Video's tile is a picture only */
    if (s->photos >= 2) {   /* the big photo's arrows and which of how many */
        const int ay = GL_PERF_Y + (GL_PERF_H - GL_ARROW_H) / 2, index = s->item == GALLERY_PHOTOS ? YELLOW : CREAM;
        arrow(&t, GL_PERF_X + 10, ay, 1, index);
        arrow(&t, GL_PERF_X + GL_PERF_W - 10 - GL_ARROW_W, ay, 0, index);
    }
    if (!s->setup) general(p);
    history(p, &t);
    if (s->setup || p->moving) setup_panel(&t, p);
    else setup_tile(&t, p);
}

static int small_measure(void *ctx, const char *s, int len)
{
    (void)ctx;
    char run[MOD_INI_DESCRIPTION_MAX + 1];
    snprintf(run, sizeof run, "%.*s", len, s);
    return text_small_width(run);
}

/* the head (name, year, author and version) and the description in lines for the tile */
static void put_lines(Page *p, const char *text, int width)
{
    TextLine lines[HISTORY_LINES];
    const int n = text_wrap(text, width, small_measure, NULL, lines, HISTORY_LINES - p->line_count);
    for (int i = 0; i < n && p->line_count < HISTORY_LINES; i++) {
        p->lines[p->line_count].text = text + lines[i].start;
        p->lines[p->line_count++].len = lines[i].len;
    }
}

void added_gallery_history_make(Page *p)
{
    char name_[64], author[64], version[32], year[32];
    text_small_words(p->card.info.name, name_, sizeof name_);
    text_small_words(p->card.info.author, author, sizeof author);
    text_small_words(p->card.info.version, version, sizeof version);
    text_small_words(p->card.info.year, year, sizeof year);
    /* the small face has no comma (its 0x2c is a u with two dots) nor a long dash, so the head's pieces are told apart by hyphens */
    int n = snprintf(p->head, sizeof p->head, "%s", name_);
    if (year[0] != 0) n += snprintf(p->head + n, sizeof p->head - (size_t)n, " (%s)", year);
    if (author[0] != 0) n += snprintf(p->head + n, sizeof p->head - (size_t)n, " - by %s", author);
    if (version[0] != 0) snprintf(p->head + n, sizeof p->head - (size_t)n, " - v%s", version);
    text_small_words(p->card.info.description, p->body, sizeof p->body);
    p->line_count = 0;
    const int width = GL_HIST_W - 8 - 12;
    put_lines(p, p->head, width);
    put_lines(p, p->body, width);
}
