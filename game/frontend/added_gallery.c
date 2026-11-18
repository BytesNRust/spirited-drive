/* See added_gallery.h. */
#include "game/frontend/added_gallery.h"

static int round_step(int v, int count, int step) { return count > 0 ? ((v + step) % count + count) % count : 0; }

int added_gallery_colour_on(const CarPieceCard *c, const GalleryState *s)
{
    return car_pieces_colour_on(c, s->skin);
}

/* the order Up and Down visit the items in (the page's tiles, then its bottom row); Color is a row of the SETUP panel now */
enum { VISITED = 7 };
static const int ORDER[VISITED] = { GALLERY_SETUP, GALLERY_PHOTOS, GALLERY_RECORDS, GALLERY_HISTORY, GALLERY_PREVIOUS, GALLERY_NEXT, GALLERY_DONE };

static int order_at(int item)
{
    for (int i = 0; i < VISITED; i++)
        if (ORDER[i] == item) return i;
    return 0;
}

/* an item the keys pass over: Photos with fewer than two photos */
static int passed_over(const GalleryState *s, int item) { return item == GALLERY_PHOTOS && s->photos < 2; }

/* the item `step` (1 down, -1 up) from `item` */
static int item_step(const GalleryState *s, int step)
{
    int at = round_step(order_at(s->item), VISITED, step);
    while (passed_over(s, ORDER[at])) at = round_step(at, VISITED, step);
    return ORDER[at];
}

/* 1 when the panel's row `row` takes keys: Color needs a skin with masks and a look that can change, Skin a look that can change and a
 * choice of skins, ABS and traction control a car that offers them */
static int row_on(const CarPieceCard *c, const GalleryState *s, int row)
{
    switch (row) {
    case SETUP_COLOUR: return !s->fixed && added_gallery_colour_on(c, s);
    case SETUP_SKIN: return !s->fixed && c->skins > 1;
    case SETUP_TRANSMISSION: return 1;
    case SETUP_ABS: return s->abs != SETUP_NOT_OFFERED;
    case SETUP_TRACTION: return s->traction != SETUP_NOT_OFFERED;
    default: return 0;
    }
}

int added_gallery_setup_row_on(const CarPieceCard *c, const GalleryState *s, int row) { return row_on(c, s, row); }

/* the row `step` (1 down, -1 up) from `row` that takes keys, `row` itself when none does */
static int row_step(const CarPieceCard *c, const GalleryState *s, int step)
{
    int row = s->row;
    for (int i = 0; i < SETUP_ROWS; i++) {
        row = round_step(row, SETUP_ROWS, step);
        if (row_on(c, s, row)) return row;
    }
    return s->row;
}

/* the panel's row stepped (1 right, -1 left) */
static void setup_value_step(const CarPieceCard *c, GalleryState *s, int step)
{
    if (!row_on(c, s, s->row)) return;
    switch (s->row) {
    case SETUP_COLOUR: s->paint = round_step(s->paint, 1 + c->info.paint_count, step); break;
    case SETUP_SKIN: s->skin = round_step(s->skin, c->skins, step); break;
    case SETUP_TRANSMISSION: s->transmission ^= 1; break;
    case SETUP_ABS: s->abs ^= 1; break;
    case SETUP_TRACTION: s->traction ^= 1; break;
    default: break;
    }
}

/* the selector or the bottom row under the item stepped (1 right, -1 left) */
static void value_step(const CarPieceCard *c, GalleryState *s, int step)
{
    (void)c;
    if (s->item == GALLERY_PHOTOS) s->photo = round_step(s->photo, s->photos, step);
    else if (s->item == GALLERY_HISTORY) s->scroll = s->scroll + step * GALLERY_SCROLL_KEY_PX < 0 ? 0 : s->scroll + step * GALLERY_SCROLL_KEY_PX;
    else if (s->item >= GALLERY_PREVIOUS && s->item <= GALLERY_DONE) s->item = GALLERY_PREVIOUS + round_step(s->item - GALLERY_PREVIOUS, 3, step);
}

/* the panel opened on its first row that takes keys */
static void setup_open(const CarPieceCard *c, GalleryState *s)
{
    s->setup = 1;
    s->row = SETUP_ROWS - 1;
    s->row = row_step(c, s, 1);
}

void added_gallery_enter(GalleryState *s, int entry)
{
    if (entry != 0) s->item = entry > 0 ? GALLERY_NEXT : GALLERY_PREVIOUS;
}

int added_gallery_input(const CarPieceCard *c, GalleryState *s, int key)
{
    if (s->skin < 0 || s->skin >= c->skins) s->skin = 0;   /* a piece that changed since the pick */
    if (s->paint < 0 || s->paint > c->info.paint_count) s->paint = 0;
    if (s->photos > 0 && (s->photo < 0 || s->photo >= s->photos)) s->photo = 0;
    if (s->scroll < 0) s->scroll = 0;
    if (s->row < 0 || s->row >= SETUP_ROWS) s->row = 0;
    if (s->item < 0 || s->item >= GALLERY_ITEMS || s->item == GALLERY_COLOUR) s->item = GALLERY_SETUP;
    if (s->setup) {   /* the panel's: Escape closes it, not the page */
        switch (key) {
        case GALLERY_KEY_ESCAPE: s->setup = 0; break;
        case GALLERY_KEY_UP: s->row = row_step(c, s, -1); break;
        case GALLERY_KEY_DOWN: s->row = row_step(c, s, 1); break;
        case GALLERY_KEY_LEFT: setup_value_step(c, s, -1); break;
        case GALLERY_KEY_RIGHT:
        case GALLERY_KEY_ENTER: setup_value_step(c, s, 1); break;
        default: break;
        }
        return GALLERY_STAY;
    }
    switch (key) {
    case GALLERY_KEY_ESCAPE: return GALLERY_LEAVE;
    case GALLERY_KEY_UP: s->item = item_step(s, -1); break;
    case GALLERY_KEY_DOWN: s->item = item_step(s, 1); break;
    case GALLERY_KEY_LEFT: value_step(c, s, -1); break;
    case GALLERY_KEY_RIGHT: value_step(c, s, 1); break;
    case GALLERY_KEY_ENTER:
        if (s->item == GALLERY_SETUP) setup_open(c, s);
        else if (s->item == GALLERY_HISTORY) break;   /* nothing to take */
        else if (s->item == GALLERY_RECORDS) return GALLERY_OPEN_RECORDS;
        else if (s->item == GALLERY_DONE) return GALLERY_LEAVE;
        else if (s->item == GALLERY_PREVIOUS) return GALLERY_GO_PREVIOUS;
        else if (s->item == GALLERY_NEXT) return GALLERY_GO_NEXT;
        else value_step(c, s, 1);
        break;
    default: break;
    }
    return GALLERY_STAY;
}

void added_gallery_scroll_touch(GalleryScroll *t, unsigned now)
{
    t->since = now;
    t->started = 1;
}

int added_gallery_scroll_auto(GalleryScroll *t, unsigned now, int *scroll, int lines, int visible, int paused)
{
    const int last = lines > visible ? lines - visible : 0;
    const int was = *scroll;
    if (*scroll > last) *scroll = last;
    if (*scroll < 0) *scroll = 0;
    if (!t->started || paused || last == 0) {
        added_gallery_scroll_touch(t, now);
        return *scroll != was;
    }
    const unsigned elapsed = now - t->since;
    if (*scroll < last) {
        if (*scroll == 0) {
            if (elapsed >= GALLERY_SCROLL_WAIT_MS) *scroll = 1, t->since = now;
        } else {
            const int moves = (int)(elapsed / GALLERY_SCROLL_MS_PER_PX);
            if (moves > 0) {
                *scroll = *scroll + moves > last ? last : *scroll + moves;
                t->since = *scroll == last ? now : t->since + (unsigned)moves * GALLERY_SCROLL_MS_PER_PX;
            }
        }
    } else if (elapsed >= GALLERY_SCROLL_END_MS) {
        *scroll = 0;
        t->since = now;
    }
    return *scroll != was;
}
