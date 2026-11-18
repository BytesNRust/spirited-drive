/* See added_gallery.h. */
#include "game/frontend/added_gallery.h"

static int round_step(int v, int count, int step) { return count > 0 ? ((v + step) % count + count) % count : 0; }

int added_gallery_colour_on(const CarPieceCard *c, const GalleryState *s)
{
    return car_pieces_colour_on(c, s->skin);
}

/* the order Up and Down visit the items in (the page's tiles, then its bottom row) */
static const int ORDER[GALLERY_ITEMS] = { GALLERY_SKIN, GALLERY_COLOUR, GALLERY_PHOTOS, GALLERY_HISTORY, GALLERY_PREVIOUS, GALLERY_NEXT,
                                          GALLERY_DONE };

static int order_at(int item)
{
    for (int i = 0; i < GALLERY_ITEMS; i++)
        if (ORDER[i] == item) return i;
    return 0;
}

/* an item the keys pass over: both boxes on a fixed page, Color while it is off, Photos with fewer than two photos */
static int passed_over(const CarPieceCard *c, const GalleryState *s, int item)
{
    return (item == GALLERY_COLOUR && (s->fixed || !added_gallery_colour_on(c, s))) || (item == GALLERY_SKIN && s->fixed) ||
           (item == GALLERY_PHOTOS && s->photos < 2);
}

/* the item `step` (1 down, -1 up) from `item` */
static int item_step(const CarPieceCard *c, const GalleryState *s, int step)
{
    int at = round_step(order_at(s->item), GALLERY_ITEMS, step);
    while (passed_over(c, s, ORDER[at])) at = round_step(at, GALLERY_ITEMS, step);
    return ORDER[at];
}

/* the selector or the bottom row under the item stepped (1 right, -1 left) */
static void value_step(const CarPieceCard *c, GalleryState *s, int step)
{
    if (s->item == GALLERY_SKIN) s->skin = round_step(s->skin, c->skins, step);
    else if (s->item == GALLERY_COLOUR && added_gallery_colour_on(c, s)) s->paint = round_step(s->paint, 1 + c->info.paint_count, step);
    else if (s->item == GALLERY_PHOTOS) s->photo = round_step(s->photo, s->photos, step);
    else if (s->item == GALLERY_HISTORY) s->scroll = s->scroll + step < 0 ? 0 : s->scroll + step;
    else if (s->item >= GALLERY_PREVIOUS && s->item <= GALLERY_DONE) s->item = GALLERY_PREVIOUS + round_step(s->item - GALLERY_PREVIOUS, 3, step);
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
    if (s->item < 0 || s->item >= GALLERY_ITEMS) s->item = s->fixed ? GALLERY_NEXT : GALLERY_SKIN;
    if (s->fixed && (s->item == GALLERY_SKIN || s->item == GALLERY_COLOUR)) {   /* port (dep59 section 8): a fixed page's boxes take no key */
        s->item = GALLERY_NEXT;
        return key == GALLERY_KEY_ESCAPE ? GALLERY_LEAVE : GALLERY_STAY;
    }
    switch (key) {
    case GALLERY_KEY_ESCAPE: return GALLERY_LEAVE;
    case GALLERY_KEY_UP: s->item = item_step(c, s, -1); break;
    case GALLERY_KEY_DOWN: s->item = item_step(c, s, 1); break;
    case GALLERY_KEY_LEFT: value_step(c, s, -1); break;
    case GALLERY_KEY_RIGHT: value_step(c, s, 1); break;
    case GALLERY_KEY_ENTER:
        if (s->item == GALLERY_HISTORY) break;   /* nothing to take */
        if (s->item == GALLERY_DONE) return GALLERY_LEAVE;
        if (s->item == GALLERY_PREVIOUS) return GALLERY_GO_PREVIOUS;
        if (s->item == GALLERY_NEXT) return GALLERY_GO_NEXT;
        value_step(c, s, 1);
        break;
    default: break;
    }
    return GALLERY_STAY;
}
