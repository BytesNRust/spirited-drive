/* dep59 step 3 and the showcase redesign: an added car's gallery, its state and keys (game/frontend/added_gallery.h): Up and Down go round
 * the tiles (SETUP, the big photo with several photos, History) and the bottom row; Enter on SETUP opens the panel, whose rows (Color, Skin,
 * Transmission, ABS, Traction) Up and Down visit, Left and Right and Enter change, Escape closes before it leaves; Enter on Previous / Next /
 * Done, and Escape, say what to do. */
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
    GalleryState s = { GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    CHECK_EQ_INT(added_gallery_colour_on(&c, &s), 1);
    /* Enter on the tile opens the panel on Color (the first row that takes keys); Escape closes it, not the page */
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_STAY);
    CHECK(s.setup == 1 && s.row == SETUP_COLOUR);
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ESCAPE), GALLERY_STAY);
    CHECK_EQ_INT(s.setup, 0);
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ESCAPE), GALLERY_LEAVE);   /* closed: it leaves the page */
    /* Color: 3 paints, 0 -> 1 -> 2 -> 3 -> 0 by Right and Enter, back by Left */
    added_gallery_input(&c, &s, GALLERY_KEY_ENTER);
    for (int want = 1; want <= 4; want++) {
        added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
        CHECK_EQ_INT(s.paint, want % 4);
    }
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    CHECK_EQ_INT(s.paint, 3);
    added_gallery_input(&c, &s, GALLERY_KEY_ENTER);
    CHECK_EQ_INT(s.paint, 0);
    /* Down to Skin: 3 skins go round; Color is off for a skin with no masks and is passed over going up */
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.row, SETUP_SKIN);
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.skin, 2);                                  /* skin 2 has no masks */
    CHECK_EQ_INT(added_gallery_colour_on(&c, &s), 0);
    CHECK_EQ_INT(added_gallery_setup_row_on(&c, &s, SETUP_COLOUR), 0);
    added_gallery_input(&c, &s, GALLERY_KEY_UP);
    CHECK_EQ_INT(s.row, SETUP_TRACTION);                      /* round, past Color (off), from Skin up */
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.row, SETUP_SKIN);                          /* and down again: Color off, Skin is the first */
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.skin, 0);                                  /* round */
    /* the three settings of Options: Transmission and, where offered, ABS and traction toggle */
    s.row = SETUP_TRANSMISSION;
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.transmission, 1);
    added_gallery_input(&c, &s, GALLERY_KEY_LEFT);
    CHECK_EQ_INT(s.transmission, 0);
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    CHECK_EQ_INT(s.row, SETUP_ABS);
    added_gallery_input(&c, &s, GALLERY_KEY_ENTER);
    CHECK_EQ_INT(s.abs, 1);
    added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
    added_gallery_input(&c, &s, GALLERY_KEY_ENTER);
    CHECK(s.row == SETUP_TRACTION && s.traction == 1);
    s.abs = SETUP_NOT_OFFERED;                                  /* a car with no ABS: its row is passed over and takes no key */
    CHECK_EQ_INT(added_gallery_setup_row_on(&c, &s, SETUP_ABS), 0);
    added_gallery_input(&c, &s, GALLERY_KEY_UP);
    CHECK_EQ_INT(s.row, SETUP_TRANSMISSION);
    s.row = SETUP_ABS;
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.abs, SETUP_NOT_OFFERED);
    s.traction = SETUP_NOT_OFFERED;
    CHECK_EQ_INT(added_gallery_setup_row_on(&c, &s, SETUP_TRACTION), 0);
    /* the keys do not leave while the panel is open (Done's Enter is nothing there) */
    s = (GalleryState){ GALLERY_DONE, 0, 0, 0, 0, 0, 0, 1, SETUP_TRANSMISSION, 0, 0, 0 };
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_STAY);
    CHECK_EQ_INT(s.transmission, 1);

    /* the tiles: Down from SETUP goes to History (one photo or none), or Photos (several); then the bottom row, round to SETUP */
    s = (GalleryState){ GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const int ONE[] = { GALLERY_HISTORY, GALLERY_PREVIOUS, GALLERY_NEXT, GALLERY_DONE, GALLERY_SETUP };
    for (int i = 0; i < 5; i++) {
        added_gallery_input(&c, &s, GALLERY_KEY_DOWN);
        CHECK_EQ_INT(s.item, ONE[i]);
    }
    s = (GalleryState){ GALLERY_SETUP, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0 };   /* three photos */
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
    s = (GalleryState){ GALLERY_PHOTOS, 0, 0, 0, 7, 3, -4, 0, 0, 0, 0, 0 };   /* a photo or scroll out of range is brought back */
    added_gallery_input(&c, &s, 0);
    CHECK_EQ_INT(s.photo, 0);
    CHECK_EQ_INT(s.scroll, 0);
    s = (GalleryState){ GALLERY_COLOUR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };   /* the item that was Color's: SETUP now */
    added_gallery_input(&c, &s, 0);
    CHECK_EQ_INT(s.item, GALLERY_SETUP);
    s = (GalleryState){ GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    added_gallery_input(&c, &s, GALLERY_KEY_UP);
    CHECK_EQ_INT(s.item, GALLERY_DONE);   /* round */

    s = (GalleryState){ GALLERY_PREVIOUS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };   /* the bottom row: Left and Right between its three */
    added_gallery_input(&c, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.item, GALLERY_NEXT);
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_GO_NEXT);
    s.item = GALLERY_PREVIOUS;
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_GO_PREVIOUS);
    s.item = GALLERY_DONE;
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ENTER), GALLERY_LEAVE);
    s.item = GALLERY_SETUP;
    CHECK_EQ_INT(added_gallery_input(&c, &s, GALLERY_KEY_ESCAPE), GALLERY_LEAVE);

    CarPieceCard bare = card(1, 0, 0);   /* no skins, no paints, no masks: the panel opens on Transmission, Skin and Color off */
    s = (GalleryState){ GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    added_gallery_input(&bare, &s, GALLERY_KEY_ENTER);
    CHECK_EQ_INT(s.row, SETUP_TRANSMISSION);
    CHECK_EQ_INT(added_gallery_colour_on(&bare, &s), 0);
    CarPieceCard masked = card(1, 0, 1);   /* masks but no paints: own colour only */
    s = (GalleryState){ GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 1, SETUP_COLOUR, 0, 0, 0 };
    added_gallery_input(&masked, &s, GALLERY_KEY_RIGHT);
    CHECK_EQ_INT(s.paint, 0);
    s = (GalleryState){ GALLERY_SETUP, 9, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0 };   /* a state out of range (a piece that changed) is brought back */
    added_gallery_input(&c, &s, 0);
    CHECK(s.skin == 0 && s.paint == 0);
    /* dep59 section 8: a fixed look (the Warrior's): Color and Skin shown but never changed, the three settings open to the panel */
    CarPieceCard w = card(1, 1, 0);
    GalleryState f = { GALLERY_SETUP, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    added_gallery_input(&w, &f, GALLERY_KEY_ENTER);
    CHECK_EQ_INT(f.row, SETUP_TRANSMISSION);
    added_gallery_input(&w, &f, GALLERY_KEY_RIGHT);
    CHECK(f.skin == 0 && f.paint == 1 && f.transmission == 1);
    /* owner 2026-11-17: a page entered by the carousel's Next starts on Next, by its Previous on Previous; the first entry as it was */
    s = (GalleryState){ GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    added_gallery_enter(&s, 1);
    CHECK_EQ_INT(s.item, GALLERY_NEXT);
    added_gallery_enter(&s, -1);
    CHECK_EQ_INT(s.item, GALLERY_PREVIOUS);
    s = (GalleryState){ GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    added_gallery_enter(&s, 0);
    CHECK_EQ_INT(s.item, GALLERY_SETUP);
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
