/* port (dep59 step 3): an added car's gallery (notes/dep59-step3-card-gallery-plan.md, Task 9), the page its card opens in Control
 * Central: its picture, its words and figures, and two selectors, Skin (the skin as it comes, then the piece's skins/ folders) and
 * Color (its own colour, then its paints=), the pair kept by id (car_looks.h). Color is off, grey and without arrows, when the skin has
 * no masks: a colour is painted through a skin's masks (the skin as it comes has them when the piece brings car_masks/<STEM>/); the
 * paint picked is kept while it is off, not applied. Previous and Next go round the menus' cars with a gallery (car_list.h: the Warrior
 * passed over). This is the page's state and its keys, with no drawing and no files (added_gallery_glue.c draws it). */
#ifndef TNFS_ADDED_GALLERY_H
#define TNFS_ADDED_GALLERY_H

#include "game/sys/car_pieces.h"

#include <stdint.h>


/* port (dep59 showcase, PR B): GALLERY_PHOTOS is the big photo's tile (Left and Right go round the photos), GALLERY_HISTORY the description's
 * (Left and Right scroll it). They come after the old items so the old numbers stand (the walks' marks). */
enum { GALLERY_SKIN, GALLERY_COLOUR, GALLERY_PREVIOUS, GALLERY_NEXT, GALLERY_DONE, GALLERY_PHOTOS, GALLERY_HISTORY, GALLERY_ITEMS };
/* port (dep59 showcase, PR C): the SETUP tile is the item that was Skin's (GALLERY_SKIN, 0: a name the walks' marks keep); Color's item is no
 * longer visited, its choice is a row of the SETUP panel */
#define GALLERY_SETUP GALLERY_SKIN
/* the SETUP panel's rows in order: the picks of the car and the three settings Options keeps (transmission, ABS, traction control) */
enum { SETUP_COLOUR, SETUP_SKIN, SETUP_TRANSMISSION, SETUP_ABS, SETUP_TRACTION, SETUP_ROWS };
enum { SETUP_NOT_OFFERED = 2 };   /* a setting the car's does not offer (Options' 2) */
/* the keys the page takes: the front end's (showcase.c's) */
enum { GALLERY_KEY_ENTER = 0x0d, GALLERY_KEY_ESCAPE = 0x1b, GALLERY_KEY_UP = 0x4800, GALLERY_KEY_LEFT = 0x4b00, GALLERY_KEY_RIGHT = 0x4d00,
       GALLERY_KEY_DOWN = 0x5000 };
/* what added_gallery_input asks for */
enum { GALLERY_STAY = 0, GALLERY_LEAVE = 1, GALLERY_GO_PREVIOUS = 2, GALLERY_GO_NEXT = 3 };

typedef struct {
    int item;    /* GALLERY_* */
    int skin;    /* 0 as it comes, k the k-th of skin[] */
    int paint;   /* 0 its own colour, k the k-th of paints= */
    int fixed;   /* port (dep59 section 8): 1 the look cannot change (the Warrior's): Skin and Color shown, never visited nor stepped */
    int photo;   /* port (PR B): the big photo shown, 0 .. photos - 1 */
    int photos;  /* port (PR B): how many big photos the page has (the glue's: 0 none, only a placeholder); with fewer than two Photos is not visited */
    int scroll;  /* port (PR B): the description's first pixel row shown (0 at the top), so it scrolls smoothly; the glue keeps it within the text */
    int setup;   /* port (PR C): 1 while the SETUP panel is open: the keys then go to its rows */
    int row;     /* port (PR C): the panel's row the keys are on, SETUP_* */
    int transmission, abs, traction;   /* port (PR C): Options' settings as the glue read them (0 or 1; SETUP_NOT_OFFERED for ABS or traction not offered) */
} GalleryState;

/* port (PR B, owner 2026-11-18): the description's automatic scroll, a pixel row at a time: it waits GALLERY_SCROLL_WAIT_MS at the top, then goes
 * a pixel row every GALLERY_SCROLL_MS_PER_PX, waits GALLERY_SCROLL_END_MS at the end and returns to the top; it stops while the pointer or the keys
 * are on it. By hand the keys and a wheel notch move it GALLERY_SCROLL_KEY_PX and GALLERY_SCROLL_WHEEL_PX rows */
enum { GALLERY_SCROLL_WAIT_MS = 3000, GALLERY_SCROLL_MS_PER_PX = 60, GALLERY_SCROLL_END_MS = 3000, GALLERY_SCROLL_KEY_PX = 12,
       GALLERY_SCROLL_WHEEL_PX = 12 };
typedef struct {
    unsigned since;   /* when the scroll last moved, was reset or the pause began */
    int started;
} GalleryScroll;
/* the scroll at `now` (ms): *scroll set (within 0 .. lines - visible; 0 when the text fits); 1 when it changed. `paused`: the pointer or the keys are on
 * the text (the time they spend does not count); a scroll moved by hand: `gallery_scroll_touch` first */
int added_gallery_scroll_auto(GalleryScroll *t, unsigned now, int *scroll, int lines, int visible, int paused);
void added_gallery_scroll_touch(GalleryScroll *t, unsigned now);

/* 1 when Color applies: the skin picked has masks */
int added_gallery_colour_on(const CarPieceCard *c, const GalleryState *s);
/* 1 when the SETUP panel's row `row` takes keys (SETUP_*) */
int added_gallery_setup_row_on(const CarPieceCard *c, const GalleryState *s, int row);
/* the page entered by a step of the carousel (`entry` 1 Next, -1 Previous; 0 the first entry, from Control Central) */
void added_gallery_enter(GalleryState *s, int entry);
/* a key: the state stepped; GALLERY_STAY, or GALLERY_LEAVE (Done, Escape), GALLERY_GO_PREVIOUS / NEXT */
int added_gallery_input(const CarPieceCard *c, GalleryState *s, int key);

#endif
