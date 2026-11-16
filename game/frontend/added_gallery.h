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


enum { GALLERY_SKIN, GALLERY_COLOUR, GALLERY_PREVIOUS, GALLERY_NEXT, GALLERY_DONE, GALLERY_ITEMS };
/* the keys the page takes: the front end's (showcase.c's) */
enum { GALLERY_KEY_ENTER = 0x0d, GALLERY_KEY_ESCAPE = 0x1b, GALLERY_KEY_UP = 0x4800, GALLERY_KEY_LEFT = 0x4b00, GALLERY_KEY_RIGHT = 0x4d00,
       GALLERY_KEY_DOWN = 0x5000 };
/* what added_gallery_input asks for */
enum { GALLERY_STAY = 0, GALLERY_LEAVE = 1, GALLERY_GO_PREVIOUS = 2, GALLERY_GO_NEXT = 3 };

typedef struct {
    int item;    /* GALLERY_* */
    int skin;    /* 0 as it comes, k the k-th of skin[] */
    int paint;   /* 0 its own colour, k the k-th of paints= */
} GalleryState;

/* 1 when Color applies: the skin picked has masks */
int added_gallery_colour_on(const CarPieceCard *c, const GalleryState *s);
/* a key: the state stepped; GALLERY_STAY, or GALLERY_LEAVE (Done, Escape), GALLERY_GO_PREVIOUS / NEXT */
int added_gallery_input(const CarPieceCard *c, GalleryState *s, int key);

#endif
