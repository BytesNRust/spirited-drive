/* dep59 step 3: an added car's gallery, its state and keys (game/frontend/added_gallery.h): Right and Left go round the skins and the
 * paints; Color does nothing and is passed over while the skin has no masks, its paint kept; Up and Down go round the items; Enter on
 * Previous / Next / Done, and Escape, say what to do. */
#include "game/frontend/added_gallery.h"
#include "test.h"

#include <string.h>

static CarPieceCard card(int skins, int paints, int own_masked)
{
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = skins;
    for (int k = 1; k < skins; k++) c.skin[k][0] = (char)('a' + k), c.skin_masked[k] = k % 2;   /* 1 masked, 2 not, 3 masked */
    c.skin_masked[0] = (uint8_t)own_masked;
    c.info.paint_count = paints;
    return c;
}

int main(void)
{
    CarPieceCard c = card(3, 3, 1);
    GalleryState s = { GALLERY_SKIN, 0, 0, 0, 0, 0, 0 };
    CHECK_EQ_INT(added_gallery_colour_on(&c, &s), 1);
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_RIGHT), GALLERY_STAY);
    CHECK_EQ_INT(s.skin, 1);
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.skin, 2);
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.skin, 0);   /* round */
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    CHECK_EQ_INT(s.skin, 2);
    CHECK_EQ_INT(added_gallery_colour_on(&c, &s), 0);   /* skin 2 has no masks */

    s = (GalleryState){ GALLERY_COLOUR, 0, 0, 0, 0, 0, 0 };   /* 3 paints: 0 -> 1 -> 2 -> 3 -> 0 */
    for (int want = 1; want <= 4; want++) {
        added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
        CHECK_EQ_INT(s.paint, want % 4);
    }
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    CHECK_EQ_INT(s.paint, 3);
    s = (GalleryState){ GALLERY_COLOUR, 2, 1, 0, 0, 0, 0 };   /* off: nothing changes, the paint kept */
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.paint, 1);
    s = (GalleryState){ GALLERY_SKIN, 2, 1, 0, 0, 0, 0 };     /* Down passes Color over while it is off, and Photos with fewer than two photos */
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.item, GALLERY_HISTORY);
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.item, GALLERY_PREVIOUS);
    added_gallery_input(&c, &s, GALLERY_KEY_UP);
    CHECK_EQ_INT(s.item, GALLERY_HISTORY);
    added_gallery_input(&c, &s, GALLERY_KEY_UP);
    CHECK_EQ_INT(s.item, GALLERY_SKIN);
    s = (GalleryState){ GALLERY_SKIN, 0, 0, 0, 0, 3, 0 };   /* three photos: Down from Color lands on Photos, then History */
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.item, GALLERY_COLOUR);
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.item, GALLERY_PHOTOS);
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);   /* Right and Left go round the photos */
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.photo, 2);
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.photo, 0);
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    CHECK_EQ_INT(s.photo, 2);
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.item, GALLERY_HISTORY);
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);   /* on History they scroll the description, never above its top */
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.scroll, 2);
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    CHECK_EQ_INT(s.scroll, 0);
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_STAY);   /* Enter on History takes nothing */
    s = (GalleryState){ GALLERY_PHOTOS, 0, 0, 0, 7, 3, -4 };   /* a photo or scroll out of range is brought back */
    added_gallery_input(&c, &s, 0);
    CHECK_EQ_INT(s.photo, 0);
    CHECK_EQ_INT(s.scroll, 0);
    s = (GalleryState){ GALLERY_SKIN, 0, 0, 0, 0, 0, 0 };
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.item, GALLERY_COLOUR);
    added_gallery_input(&c, &s, GALLERY_KEY_UP);
    added_gallery_input(&c, &s, GALLERY_KEY_UP);
    CHECK_EQ_INT(s.item, GALLERY_DONE);   /* round */

    s = (GalleryState){ GALLERY_PREVIOUS, 0, 0, 0, 0, 0, 0 };   /* the bottom row: Left and Right between its three */
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.item, GALLERY_NEXT);
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_GO_NEXT);
    s.item = GALLERY_PREVIOUS;
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_GO_PREVIOUS);
    s.item = GALLERY_DONE;
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_LEAVE);
    s.item = GALLERY_SKIN;
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ESCAPE), GALLERY_LEAVE);
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_STAY);   /* Enter on a selector steps it */
    CHECK_EQ_INT(s.skin, 1);

    CarPieceCard bare = card(1, 0, 0);   /* no skins, no paints, no masks */
    s = (GalleryState){ GALLERY_SKIN, 0, 0, 0, 0, 0, 0 };
    added_gallery_input(&bare, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.skin, 0);
    CHECK_EQ_INT(added_gallery_colour_on(&bare, &s), 0);
    s = (GalleryState){ GALLERY_COLOUR, 0, 0, 0, 0, 0, 0 };
    CarPieceCard masked = card(1, 0, 1);   /* masks but no paints: own colour only */
    added_gallery_input(&masked, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.paint, 0);
    s = (GalleryState){ GALLERY_SKIN, 9, 40, 0, 0, 0, 0 };   /* a state out of range (a piece that changed) is brought back */
    added_gallery_input(&c, &s, 0);
    CHECK(s.skin == 0 && s.paint == 0);
    /* dep59 section 8: a fixed look (the Warrior's): Skin and Color shown but never changed nor visited, keys on them doing nothing */
    CarPieceCard w = card(1, 1, 0);
    GalleryState f = { GALLERY_SKIN, 0, 1, 1, 0, 0, 0 };
    CHECK_EQ_INT(added_gallery_input(&w, &f, GALLERY_KEY_RIGHT), GALLERY_STAY);
    CHECK(f.skin == 0 && f.paint == 1);
    CHECK_EQ_INT(f.item, GALLERY_NEXT);   /* a fixed page's item is never a box: brought to Next */
    f.item = GALLERY_COLOUR;
    added_gallery_input(&w, &f, GALLERY_KEY_LEFT);
    CHECK(f.paint == 1 && f.item == GALLERY_NEXT);
    f.item = GALLERY_COLOUR;
    CHECK_EQ_INT(added_gallery_input(&w, &f, GALLERY_KEY_ENTER), GALLERY_STAY);   /* a key on a box does nothing, Enter too */
    CHECK(f.paint == 1 && f.item == GALLERY_NEXT);
    f.item = GALLERY_NEXT;
    added_gallery_input(&w, &f, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(f.item, GALLERY_DONE);
    added_gallery_input(&w, &f, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(f.item, GALLERY_HISTORY);    /* round, past Skin, Color and Photos (one photo or none) */
    added_gallery_input(&w, &f, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(f.item, GALLERY_PREVIOUS);
    added_gallery_input(&w, &f, GALLERY_KEY_UP);
    CHECK_EQ_INT(f.item, GALLERY_HISTORY);    /* and back, past them */
    f.item = GALLERY_DONE;
    added_gallery_input(&w, &f, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(f.item, GALLERY_PREVIOUS);   /* the bottom row as on any page */
    CHECK(f.skin == 0 && f.paint == 1);
    /* owner 2026-11-17: a page entered by the carousel's Next starts on Next, by its Previous on Previous; the first entry as it was */
    s = (GalleryState){ GALLERY_SKIN, 0, 0, 0, 0, 0, 0 };
    added_gallery_enter(&s, 1);
    CHECK_EQ_INT(s.item, GALLERY_NEXT);
    added_gallery_enter(&s, -1);
    CHECK_EQ_INT(s.item, GALLERY_PREVIOUS);
    s = (GalleryState){ GALLERY_SKIN, 0, 0, 0, 0, 0, 0 };
    added_gallery_enter(&s, 0);
    CHECK_EQ_INT(s.item, GALLERY_SKIN);
    s = (GalleryState){ GALLERY_NEXT, 0, 1, 1, 0, 0, 0 };   /* the Warrior's (fixed) */
    added_gallery_enter(&s, -1);
    CHECK_EQ_INT(s.item, GALLERY_PREVIOUS);
    /* the description's automatic scroll: 3 s at the top, a line each 2 s, 3 s at the end, then the top again; paused it waits */
    GalleryScroll g = { 0, 0 };
    int line = 0;
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 1000, &line, 6, 3, 0), 0);   /* the first call starts the clock */
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 3999, &line, 6, 3, 0), 0);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 4000, &line, 6, 3, 0), 1);
    CHECK_EQ_INT(line, 1);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 5999, &line, 6, 3, 0), 0);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 6000, &line, 6, 3, 0), 1);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 8000, &line, 6, 3, 0), 1);
    CHECK_EQ_INT(line, 3);                                                  /* the last it can show: 6 lines, 3 seen */
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 10999, &line, 6, 3, 0), 0);  /* it waits at the end */
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 11000, &line, 6, 3, 0), 1);
    CHECK_EQ_INT(line, 0);                                                  /* and goes back up */
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 13999, &line, 6, 3, 0), 0);  /* then waits at the top again */
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 14000, &line, 6, 3, 0), 1);
    CHECK_EQ_INT(line, 1);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 20000, &line, 6, 3, 1), 0);  /* paused: nothing moves, and the wait starts over */
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 21999, &line, 6, 3, 0), 0);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 22000, &line, 6, 3, 0), 1);
    CHECK_EQ_INT(line, 2);
    line = 9;                                                               /* a scroll past the end (a text that changed) is brought back */
    added_gallery_scroll_touch(&g, 30000);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 30001, &line, 6, 3, 0), 1);
    CHECK_EQ_INT(line, 3);
    line = 2;                                                               /* a text that fits: the top, never moved */
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 40000, &line, 2, 3, 0), 1);
    CHECK_EQ_INT(line, 0);
    CHECK_EQ_INT(added_gallery_scroll_auto(&g, 60000, &line, 2, 3, 0), 0);
    return TEST_REPORT();
}
