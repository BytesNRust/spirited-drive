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
    int scroll;  /* port (PR B): the description's first line shown (0 at the top); the glue keeps it within the text */
} GalleryState;

/* port (PR B): the description's automatic scroll: it waits GALLERY_SCROLL_WAIT_MS at the top, then goes a line every GALLERY_SCROLL_STEP_MS, waits
 * GALLERY_SCROLL_END_MS at the end and returns to the top; it stops while the pointer or the keys are on it */
enum { GALLERY_SCROLL_WAIT_MS = 3000, GALLERY_SCROLL_STEP_MS = 2000, GALLERY_SCROLL_END_MS = 3000 };
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
/* the page entered by a step of the carousel (`entry` 1 Next, -1 Previous; 0 the first entry, from Control Central) */
void added_gallery_enter(GalleryState *s, int entry);
/* a key: the state stepped; GALLERY_STAY, or GALLERY_LEAVE (Done, Escape), GALLERY_GO_PREVIOUS / NEXT */
int added_gallery_input(const CarPieceCard *c, GalleryState *s, int key);

#endif
