/* port (dep59 step 3): an added car's gallery page run (the Warrior's too): shown, its keys and clicks taken until it is left or another
 * car is asked for, the keys and the button let go before it closes. The drawing, the input and the screen are the ops' (added_gallery_glue.c);
 * this is the order they are called in. */
#ifndef TNFS_ADDED_GALLERY_RUN_H
#define TNFS_ADDED_GALLERY_RUN_H

#include "game/frontend/added_gallery.h"

enum { GALLERY_RELEASE_FRAMES = 300 };   /* a key or button held longer than this (5 s at 60 Hz) is let be */

typedef struct GalleryRunOps {
    void (*clear)(void *ctx);                         /* both pictures cleared */
    void (*black)(void *ctx);                         /* the palette on screen all black */
    void (*draw)(void *ctx, const GalleryState *s);   /* the page as `s` says into the back picture, its palette made (not shown) */
    void (*palette)(void *ctx);                       /* the page's palette on screen at once */
    void (*flip)(void *ctx, const GalleryState *s);   /* the back picture on screen, the pointer on */
    void (*fade_in)(void *ctx);                       /* the palette faded in to the page's */
    void (*fade_out)(void *ctx);                      /* the palette faded out to black */
    int (*key)(void *ctx);                            /* the key down, 0 none; first each frame: it pumps the events */
    int (*buttons)(void *ctx);                        /* the pointer's buttons (POINTER_BUTTON_*) */
    void (*pointer)(void *ctx, int *x, int *y);       /* where the pointer is */
    int (*click)(void *ctx, GalleryState *s);         /* a click: the key it stands for (the item moved to the one clicked), 0 none */
    void (*music)(void *ctx);                         /* the music's stream serviced (the showcase's plays on here) */
    void (*wait)(void *ctx);                          /* the frame's end */
    int (*wheel)(void *ctx);                          /* the wheel's notches since the last call, + up, - down */
    unsigned (*now)(void *ctx);                       /* the time in ms */
    int (*lines)(void *ctx, int *visible);            /* the description's lines in all, and how many the tile shows (0, 0 none) */
    void *ctx;
} GalleryRunOps;

/* the page until it is left or another car is asked for: GALLERY_LEAVE, GALLERY_GO_PREVIOUS or GALLERY_GO_NEXT */
int added_gallery_run(const CarPieceCard *c, GalleryState *s, const GalleryRunOps *o);

#endif
