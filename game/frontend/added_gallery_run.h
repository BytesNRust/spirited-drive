/* port (dep59 step 3): an added car's gallery page run (the Warrior's too): shown, its keys and clicks taken until it is left or another
 * car is asked for, the keys and the button let go before it closes. The drawing, the input and the screen are the ops' (added_gallery_glue.c);
 * this is the order they are called in. */
#ifndef TNFS_ADDED_GALLERY_RUN_H
#define TNFS_ADDED_GALLERY_RUN_H

#include "game/frontend/added_gallery.h"

enum { GALLERY_RELEASE_FRAMES = 300 };   /* a key or button held longer than this (5 s at 60 Hz) is let be */

typedef struct GalleryRunOps {
    void (*show)(void *ctx, const GalleryState *s);   /* the page as `s` says: its palette, drawn, flipped, the pointer on */
    int (*key)(void *ctx);                            /* the key down, 0 none; first each frame: it pumps the events */
    int (*buttons)(void *ctx);                        /* the pointer's buttons (POINTER_BUTTON_*) */
    int (*click)(void *ctx, GalleryState *s);         /* a click: the key it stands for (the item moved to the one clicked), 0 none */
    void (*music)(void *ctx);                         /* the music's stream serviced (the showcase's plays on here) */
    void (*wait)(void *ctx);                          /* the frame's end */
    void *ctx;
} GalleryRunOps;

/* the page until it is left or another car is asked for: GALLERY_LEAVE, GALLERY_GO_PREVIOUS or GALLERY_GO_NEXT */
int added_gallery_run(const CarPieceCard *c, GalleryState *s, const GalleryRunOps *o);

#endif
