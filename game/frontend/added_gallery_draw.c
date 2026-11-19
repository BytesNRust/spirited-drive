/* See added_gallery_page.h: an added car's gallery page, what is drawn over its still part (dep59 showcase, PR B): the name, the tiles' labels,
 * the focus frame, the big photo's arrows and count, General's figures, History's text, the Color tile's two rows. */
#include "game/frontend/added_gallery_page.h"

#include "game/frontend/added_card.h"
#include "game/frontend/gallery_label.h"
#include "game/sys/palette.h"
#include "game/sys/text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *to;
    int w, h;
    const uint8_t *palette;   /* the page's: the labels' edges are mixed in it */
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

void added_gallery_draw_name(const Page *p) { name(p); }

void added_gallery_draw_title(const Page *p)
{
    name(p);
    text_select(TEXT_RED);
    text_draw_smooth("Car Showcase", GL_HEADING_X, GL_HEADING_Y, GL_HEADING_NUM, GL_HEADING_DEN);
}

/* a tile's label at its foot, as the CD pages have it (owner 2026-11-18): black, nothing white about it; yellow-shadowed when the tile is the item */
static void label(const Target *t, const char *text, int x, int y, int h, int lit)
{
    /* the letters' soft edges are mixed in the palette's entries, never in the SETUP panel's two previews: their colours change with the choice (Color,
     * Skin), and an edge that took one would differ between two draws of the same page */
    uint8_t keep[PALETTE_COLORS], mixing[PALETTE_COLORS];
    memcpy(keep, palette_usable(), sizeof keep);
    memcpy(mixing, keep, sizeof mixing);
    mixing[SWATCH] = mixing[SKIN_SWATCH] = 0;
    palette_usable_set(mixing);
    gallery_label_tile(t->to, t->w, t->h, t->palette, text, x, y, h, BLACK, YELLOW, lit);   /* the CD's own margins and lit colour, measured (gallery_label.h) */
    palette_usable_set(keep);
}

/* `text` cut (with a "." at its end) until the main face at the panel's size shows it in `width` pixels */
static void fitted_main(const char *text, int width, char *out, int size)
{
    snprintf(out, (size_t)size, "%s", text);
    while (out[0] != 0 && text_width_scaled(out, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN) > width) {
        const size_t n = strlen(out);
        out[n - 1] = 0;
        if (n > 1) out[n - 2] = '.';
    }
}

/* a tile row's label at the left of x0..x1 and its value at the right, both upright in the main face (owner 2026-11-19: the small face leans): both
 * at FIGURE_NUM/FIGURE_DEN, 3/4, the CD's own row size (kit E9; 7/8, the panel's, would cut a country in General's 147 px), the label dim and the value cream;
 * a value that is not there is a dash */
enum { FIGURE_NUM = 3, FIGURE_DEN = 4, FIGURE_DY = 0 };

static int figure_measure(void *ctx, const char *s)
{
    (void)ctx;
    return text_width_scaled(s, FIGURE_NUM, FIGURE_DEN);
}

/* owner 2026-11-19: a value that does not fit `room` px is cut with "..." (the cards' names are), "" when not even that fits */
void added_gallery_figure_fit(const char *value, int room, char *out, int size)
{
    added_card_name_fit(value[0] == 0 ? "-" : value, room, figure_measure, NULL, out, size);
}

static void figure_row(const char *label_, const char *value, int x0, int x1, int y)
{
    char fits[64];
    text_select(TEXT_DIM);
    text_draw_smooth(label_, x0, y, FIGURE_NUM, FIGURE_DEN);
    added_gallery_figure_fit(value, x1 - x0 - text_width_scaled(label_, FIGURE_NUM, FIGURE_DEN) - 4, fits, sizeof fits);
    text_select(TEXT_CREAM);
    text_draw_smooth(fits, x1 - text_width_scaled(fits, FIGURE_NUM, FIGURE_DEN), y + FIGURE_DY, FIGURE_NUM, FIGURE_DEN);
}

static void general(const Page *p)
{
    static const char *const LABEL[4] = { "Make", "Year", "Country", "Category" };
    const char *const value[4] = { p->card.info.make, p->card.info.year, p->card.info.country, p->card.info.category };
    for (int i = 0; i < 4; i++) figure_row(LABEL[i], value[i], GL_GEN_X + 6, GL_GEN_X + GL_GEN_W - 6, GL_GEN_Y + 6 + i * 20);
}

/* owner 2026-11-19: Mechanical's tile: the power, the weight and the weight per horsepower, in the player's units */
static void mechanical(const Page *p)
{
    static const char *const LABEL[3] = { "Power", "Weight", "Weight/power" };
    const char *const value[3] = { p->mech.power, p->mech.weight, p->mech.ratio };
    for (int i = 0; i < 3; i++) figure_row(LABEL[i], value[i], GL_MECH_X + 6, GL_MECH_X + GL_MECH_W - 6, GL_MECH_Y + 5 + i * 19);
}

/* owner 2026-11-18: the History text in the main face at 3/4, scrolled a pixel row at a time */
enum { HIST_NUM = 3, HIST_DEN = 4 };

int added_gallery_history_line_px(void) { return text_line_height() * HIST_NUM / HIST_DEN; }

int added_gallery_history_window(void) { return GL_HIST_H - 8 - 36; }   /* ends above the label, the margin the top has */

static void history(const Page *p, const Target *t)
{
    const int x = GL_HIST_X + 8, lh = added_gallery_history_line_px(), win = added_gallery_history_window();
    const int w = GL_HIST_W - 8 - 12 + 4, total = p->line_count * lh + 2;
    if (p->line_count > 0 && lh > 0) {
        uint8_t *big = calloc((size_t)w * total, 1);
        for (int i = 0; big != NULL && i < p->line_count; i++) {
            const HistoryLine *l = &p->lines[i];
            char line[160];
            snprintf(line, sizeof line, "%.*s", l->len, l->text);
            const int h = text_mask_rows(line, HIST_NUM, HIST_DEN);
            uint8_t *tmp = h > 0 ? malloc((size_t)w * h) : NULL;
            if (tmp != NULL && text_mask_scaled(line, HIST_NUM, HIST_DEN, tmp, w, h))
                for (int j = 0; j < h && i * lh + j < total; j++)
                    for (int k = 0; k < w; k++) {
                        uint8_t *const at = &big[(i * lh + j) * w + k];
                        if (tmp[j * w + k] > *at) *at = tmp[j * w + k];
                    }
            free(tmp);
        }
        const int off = p->state.scroll < 0 ? 0 : p->state.scroll, rows = total - off < win ? total - off : win;
        if (big != NULL && rows > 0) {
            text_select(TEXT_WHITE);
            text_draw_item_mask(big + (size_t)off * w, w, rows, x, GL_HIST_Y + 6);
        }
        free(big);
    }
    if (total > win) {   /* a thumb on a track at the right edge: where the text is */
        const int tx = GL_HIST_X + GL_HIST_W - 7, ty = GL_HIST_Y + 8, th = win;
        fill(t, tx, ty, tx + 2, ty + th - 1, DIM);
        const int thumb = th * win / total, at = (th - thumb) * p->state.scroll / (total - win);
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
    label(t, "SETUP", GL_COL_X, GL_COL_Y, GL_COL_H, lit);   /* at the foot like the others */
    arrow(t, GL_COL_X + GL_COL_W - 24, GL_COL_Y + (GL_COL_H - GL_ARROW_H) / 2, 0, lit ? YELLOW : CREAM);
}

/* the value the panel's row `row` shows */
static void setup_value(const Page *p, int row, char *out, int size)
{
    const GalleryState *s = &p->state;
    switch (row) {
    case SETUP_COLOUR: text_words(s->paint == 0 ? "Default" : p->card.info.paints[s->paint - 1].name, out, size); break;
    case SETUP_SKIN: text_words(s->skin == 0 ? "Default" : p->card.skin[s->skin], out, size); break;
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
    edge(t, GL_SETUP_X, GL_SETUP_Y, GL_SETUP_X + GL_SETUP_W - 1, GL_SETUP_Y + GL_SETUP_H - 1, GL_FRAME, YELLOW);   /* in focus: the frame of a lit tile */
    text_select(TEXT_RED);
    text_draw_smooth("Car Setup", GL_SETUP_X + GL_SETUP_LABEL_X, GL_SETUP_Y + 8, GL_SETUP_TITLE_NUM, GL_SETUP_TITLE_DEN);   /* the size of the CD pages' own, red */
    int ink_top, ink_bottom;
    text_ink_scaled("H", GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN, &ink_top, &ink_bottom);
    for (int row = 0; row < SETUP_ROWS; row++) {
        const int y = GL_SETUP_Y + GL_SETUP_ROW0 + row * GL_SETUP_STEP, on = added_gallery_setup_row_on(&p->card, s, row), lit = s->row == row;
        const int choice = row == SETUP_COLOUR || row == SETUP_SKIN;
        SetupChoice c;
        gallery_setup_choice(&c);   /* the second column: < box   name   >, the box next to the left arrow, the name centred between it and the right arrow */
        const int mid = y + (ink_top + ink_bottom) / 2;   /* the row's middle: the arrows and the box centre on it */
        char value[64], fits[64];
        setup_value(p, row, value, sizeof value);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_CREAM);
        text_draw_smooth(LABEL[row], GL_SETUP_X + GL_SETUP_LABEL_X, y, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_WHITE);
        if (!choice || !on) {   /* a switch's value (or a choice that is off): at the right edge of the second column, as in Options (owner 2026-11-19) */
            fitted_main(value, c.right + GL_ARROW_W - c.left, fits, sizeof fits);
            text_draw_smooth(fits, c.right + GL_ARROW_W - text_width_scaled(fits, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN), y, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
            continue;
        }
        const int index = lit ? YELLOW : CREAM;
        arrow(t, c.left, mid - GL_ARROW_H / 2, 1, index);
        arrow(t, c.right, mid - GL_ARROW_H / 2, 0, index);
        preview(t, c.preview, mid - GL_SETUP_PREVIEW_H / 2, row == SETUP_COLOUR ? SWATCH : SKIN_SWATCH);
        fitted_main(value, c.name_x1 - c.name_x0, fits, sizeof fits);   /* an added car's own long name is cut; the CD's all fit */
        text_draw_smooth(fits, c.name_x0 + (c.name_x1 - c.name_x0 - text_width_scaled(fits, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN)) / 2, y, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
    }
}

void added_gallery_draw_tiles(const Page *p, uint8_t *to, int tw, int th)
{
    const Target t = { to, tw, th, p->palette };
    const GalleryState *s = &p->state;
    /* the focus: a yellow frame round the tile the item is, over the black one */
    if (s->item == GALLERY_PHOTOS)
        edge(&t, GL_PERF_X - GL_FRAME, GL_PERF_Y - GL_FRAME, GL_PERF_X + GL_PERF_W + GL_FRAME - 1, GL_PERF_Y + GL_PERF_H + GL_FRAME - 1, GL_FRAME, YELLOW);
    if (s->item == GALLERY_HISTORY)
        edge(&t, GL_HIST_X - GL_FRAME, GL_HIST_Y - GL_FRAME, GL_HIST_X + GL_HIST_W + GL_FRAME - 1, GL_HIST_Y + GL_HIST_H + GL_FRAME - 1, GL_FRAME, YELLOW);
    if (s->item == GALLERY_SETUP && !s->setup)
        edge(&t, GL_COL_X - GL_FRAME, GL_COL_Y - GL_FRAME, GL_COL_X + GL_COL_W + GL_FRAME - 1, GL_COL_Y + GL_COL_H + GL_FRAME - 1, GL_FRAME, YELLOW);
    if (s->item == GALLERY_RECORDS)
        edge(&t, GL_VID_X - GL_FRAME, GL_VID_Y - GL_FRAME, GL_VID_X + GL_VID_W + GL_FRAME - 1, GL_VID_Y + GL_VID_H + GL_FRAME - 1, GL_FRAME, YELLOW);
    label(&t, "GALLERY", GL_PERF_X, GL_PERF_Y, GL_PERF_H, s->item == GALLERY_PHOTOS);
    label(&t, "RECORDS", GL_VID_X, GL_VID_Y, GL_VID_H, s->item == GALLERY_RECORDS);
    if (!s->setup) label(&t, "GENERAL", GL_GEN_X, GL_GEN_Y, GL_GEN_H, 0);   /* the open panel covers its top: its label would show cut */
    label(&t, "MECHANICAL", GL_MECH_X, GL_MECH_Y, GL_MECH_H, 0);
    label(&t, "HISTORY", GL_HIST_X, GL_HIST_Y, GL_HIST_H, s->item == GALLERY_HISTORY);   /* RECORDS' tile: the flag */
    if (s->photos >= 2) {   /* the big photo's arrows and which of how many */
        const int ay = GL_PERF_Y + (GL_PERF_H - GL_ARROW_H) / 2, index = s->item == GALLERY_PHOTOS ? YELLOW : CREAM;
        arrow(&t, GL_PERF_X + 10, ay, 1, index);
        arrow(&t, GL_PERF_X + GL_PERF_W - 10 - GL_ARROW_W, ay, 0, index);
    }
    if (!s->setup) general(p);
    mechanical(p);
    history(p, &t);
    if (s->setup || p->moving) setup_panel(&t, p);
    else setup_tile(&t, p);
}

static int small_measure(void *ctx, const char *s, int len)
{
    (void)ctx;
    char run[MOD_INI_DESCRIPTION_MAX + 1];
    snprintf(run, sizeof run, "%.*s", len, s);
    return text_width_scaled(run, HIST_NUM, HIST_DEN);
}

/* `text` in lines for the tile */
static void put_lines(Page *p, const char *text, int width)
{
    TextLine lines[HISTORY_LINES];
    const int n = text_wrap(text, width, small_measure, NULL, lines, HISTORY_LINES - p->line_count);
    for (int i = 0; i < n && p->line_count < HISTORY_LINES; i++) {
        p->lines[p->line_count].text = text + lines[i].start;
        p->lines[p->line_count++].len = lines[i].len;
    }
}

/* owner 2026-11-19: no heading line (the name is on the page's top): the description, a blank line and "vX - by Author" */
void added_gallery_history_make(Page *p)
{
    char author[64], version[32];
    text_words(p->card.info.author, author, sizeof author);
    text_words(p->card.info.version, version, sizeof version);
    added_gallery_history_foot(author, version, p->foot, sizeof p->foot);
    text_words(p->card.info.description, p->body, sizeof p->body);
    p->line_count = 0;
    const int width = GL_HIST_W - 8 - 12;
    put_lines(p, p->body, width);
    if (p->line_count > 0 && p->foot[0] != 0 && p->line_count < HISTORY_LINES) {   /* the blank line between them */
        p->lines[p->line_count].text = p->foot;
        p->lines[p->line_count++].len = 0;
    }
    put_lines(p, p->foot, width);
}
