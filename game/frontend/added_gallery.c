/* See added_gallery.h. */
#include "game/frontend/added_gallery.h"

static int round_step(int v, int count, int step) { return count > 0 ? ((v + step) % count + count) % count : 0; }

int added_gallery_colour_on(const CarPieceCard *c, const GalleryState *s)
{
    return s->skin >= 0 && s->skin < c->skins && s->skin <= CAR_PIECE_SKINS_MAX && c->skin_masked[s->skin] != 0;
}

/* the item `step` (1 down, -1 up) from `item`, Color passed over while it is off */
static int item_step(const CarPieceCard *c, const GalleryState *s, int step)
{
    int item = round_step(s->item, GALLERY_ITEMS, step);
    if (item == GALLERY_COLOUR && !added_gallery_colour_on(c, s)) item = round_step(item, GALLERY_ITEMS, step);
    return item;
}

/* the selector or the bottom row under the item stepped (1 right, -1 left) */
static void value_step(const CarPieceCard *c, GalleryState *s, int step)
{
    if (s->item == GALLERY_SKIN) s->skin = round_step(s->skin, c->skins, step);
    else if (s->item == GALLERY_COLOUR && added_gallery_colour_on(c, s)) s->paint = round_step(s->paint, 1 + c->info.paint_count, step);
    else if (s->item >= GALLERY_PREVIOUS) s->item = GALLERY_PREVIOUS + round_step(s->item - GALLERY_PREVIOUS, 3, step);
}

int added_gallery_input(const CarPieceCard *c, GalleryState *s, int key)
{
    if (s->skin < 0 || s->skin >= c->skins) s->skin = 0;   /* a piece that changed since the pick */
    if (s->paint < 0 || s->paint > c->info.paint_count) s->paint = 0;
    if (s->item < 0 || s->item >= GALLERY_ITEMS) s->item = GALLERY_SKIN;
    switch (key) {
    case GALLERY_KEY_ESCAPE: return GALLERY_LEAVE;
    case GALLERY_KEY_UP: s->item = item_step(c, s, -1); break;
    case GALLERY_KEY_DOWN: s->item = item_step(c, s, 1); break;
    case GALLERY_KEY_LEFT: value_step(c, s, -1); break;
    case GALLERY_KEY_RIGHT: value_step(c, s, 1); break;
    case GALLERY_KEY_ENTER:
        if (s->item == GALLERY_DONE) return GALLERY_LEAVE;
        if (s->item == GALLERY_PREVIOUS) return GALLERY_GO_PREVIOUS;
        if (s->item == GALLERY_NEXT) return GALLERY_GO_NEXT;
        value_step(c, s, 1);
        break;
    default: break;
    }
    return GALLERY_STAY;
}
