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
            text_draw_mask(leant, ow, h, GL_NAME_X, GL_NAME_Y);
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

/* a tile's label at its foot, lit when the tile is the item */
static void label(const char *text, int x, int y, int h, int lit)
{
    text_select(lit ? TEXT_YELLOW : TEXT_WHITE);
    text_draw_smooth(text, x + 8, y + h - 24, 3, 4);
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
    for (int i = 0; i < 4; i++) row(LABEL[i], value[i], GL_GEN_X + 12, GL_GEN_X + GL_GEN_W - 12, GL_GEN_Y + 6 + i * 20, TEXT_CREAM);
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

/* the SETUP tile closed: its word and an arrow, lit when it is the item */
static void setup_tile(const Target *t, const Page *p)
{
    const int lit = p->state.item == GALLERY_SETUP;
    text_select(lit ? TEXT_YELLOW : TEXT_WHITE);
    text_draw_smooth("SETUP", GL_COL_X + 14, GL_COL_Y + 20, 3, 4);
    arrow(t, GL_COL_X + GL_COL_W - 24, GL_COL_Y + (GL_COL_H - GL_ARROW_H) / 2, 0, lit ? YELLOW : CREAM);
}

/* the value the panel's row `row` shows */
static void setup_value(const Page *p, int row, char *out, int size)
{
    const GalleryState *s = &p->state;
    switch (row) {
    case SETUP_COLOUR: text_small_words(s->paint == 0 ? "Default" : p->card.info.paints[s->paint - 1].name, out, size); break;
    case SETUP_SKIN: text_small_words(s->skin == 0 ? "Default" : p->card.skin[s->skin], out, size); break;
    case SETUP_TRANSMISSION: snprintf(out, (size_t)size, "%s", s->transmission == 0 ? "Auto" : "Manual"); break;
    case SETUP_ABS: snprintf(out, (size_t)size, "%s", s->abs == SETUP_NOT_OFFERED ? "N/A" : s->abs == 0 ? "On" : "Off"); break;
    default: snprintf(out, (size_t)size, "%s", s->traction == SETUP_NOT_OFFERED ? "N/A" : s->traction == 0 ? "On" : "Off"); break;
    }
}

/* the SETUP panel open over the tile and General's top: its fill, a border, the title and the rows (the focused one yellow with its arrows,
 * one that takes no key dim) */
static void setup_panel(const Target *t, const Page *p)
{
    static const char *const LABEL[SETUP_ROWS] = { "Color", "Skin", "Transmission", "ABS", "Traction" };
    const GalleryState *s = &p->state;
    fill(t, GL_SETUP_X, GL_SETUP_Y, GL_SETUP_X + GL_SETUP_W - 1, GL_SETUP_Y + GL_SETUP_H - 1, p->panel_fill);
    edge(t, GL_SETUP_X, GL_SETUP_Y, GL_SETUP_X + GL_SETUP_W - 1, GL_SETUP_Y + GL_SETUP_H - 1, 1, CREAM);
    text_select(TEXT_YELLOW);
    text_draw_smooth("SETUP", GL_SETUP_X + 14, GL_SETUP_Y + 8, 3, 4);
    for (int row = 0; row < SETUP_ROWS; row++) {
        const int y = GL_SETUP_Y + GL_SETUP_ROW0 + row * GL_SETUP_STEP, on = added_gallery_setup_row_on(&p->card, s, row), lit = s->row == row;
        char value[64], fits[64];
        setup_value(p, row, value, sizeof value);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_CREAM);
        text_small_draw(LABEL[row], GL_SETUP_X + 16, y);
        const int vx = GL_SETUP_X + GL_SETUP_VALUE_X;
        fitted(value, GL_SETUP_VALUE_W - 2 * (GL_ARROW_W + 4), fits, sizeof fits);
        if (row == SETUP_COLOUR && on && s->paint > 0) fill(t, vx - 16, y + 3, vx - 8, y + 10, SWATCH);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_WHITE);
        text_small_draw(fits, vx + (GL_SETUP_VALUE_W - text_small_width(fits)) / 2, y);
        if (on) {
            const int index = lit ? YELLOW : CREAM;
            arrow(t, vx, y + 1, 1, index);
            arrow(t, vx + GL_SETUP_VALUE_W - GL_ARROW_W, y + 1, 0, index);
        }
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
    label("PERFORMANCE", GL_PERF_X, GL_PERF_Y, GL_PERF_H, s->item == GALLERY_PHOTOS);
    if (!s->setup) label("GENERAL", GL_GEN_X, GL_GEN_Y, GL_GEN_H, 0);   /* the open panel covers its top: its label would show cut */
    label("VIDEO", GL_VID_X, GL_VID_Y, GL_VID_H, 0);
    label("HISTORY", GL_HIST_X, GL_HIST_Y, GL_HIST_H, s->item == GALLERY_HISTORY);
    if (s->photos >= 2) {   /* the big photo's arrows and which of how many */
        const int ay = GL_PERF_Y + (GL_PERF_H - GL_ARROW_H) / 2, index = s->item == GALLERY_PHOTOS ? YELLOW : CREAM;
        arrow(&t, GL_PERF_X + 10, ay, 1, index);
        arrow(&t, GL_PERF_X + GL_PERF_W - 10 - GL_ARROW_W, ay, 0, index);
        char count[32];
        snprintf(count, sizeof count, "PHOTOS %d/%d", s->photo + 1, s->photos);
        text_select(TEXT_WHITE);
        text_small_draw(count, GL_PERF_X + GL_PERF_W - 8 - text_small_width(count), GL_PERF_Y + 6);
    }
    if (!s->setup) general(p);
    history(p, &t);
    if (s->setup) setup_panel(&t, p);
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
