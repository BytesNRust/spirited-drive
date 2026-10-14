/* An art screen (a .qfs with "!pal" and "bgnd") shown. */
#ifndef TNFS_SCREEN_SHOW_H
#define TNFS_SCREEN_SHOW_H

#include "game/frontend/palette_put.h"
#include "game/frontend/yes_no_box.h"

#include <stdint.h>

enum {
    SCREEN_SHOW_WAIT = 0,    /* faded in, waits, faded out */
    SCREEN_SHOW_STAY = 1,    /* faded in, left up */
    SCREEN_SHOW_SWITCH = 3   /* no clearing and no fade: drawn straight over */
};

typedef struct ScreenShowOps {
    void (*pointer_hide)(void *ctx);                                     /* [004c618c] */
    void (*fill)(void *ctx, uint32_t colour);                            /* 00482980 (cdecl) */
    void (*unlock)(void *ctx);                                           /* 004733b0 */
    void (*flip)(void *ctx);                                             /* 004733ec */
    int32_t (*load_art)(void *ctx, const char *name);                    /* 00466c34: eax the name; the bank */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);   /* 004732dd (cdecl) */
    void (*music_feed)(void *ctx);                                       /* 004640d0 */
    void (*select)(void *ctx, int32_t picture);                          /* 00473680 (cdecl) */
    void (*blit)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl): at its own place */
    void (*select_default)(void *ctx);                                   /* 00473604 */
    void (*blit_at)(void *ctx, int32_t shape, int32_t x, int32_t y);     /* 004737d0 (cdecl) */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*deadline_set)(void *ctx, int32_t time);                       /* 0047334f (cdecl) */
    int32_t (*deadline_passed)(void *ctx);                               /* 0047336d */
    void (*music_service)(void *ctx);                                    /* 0046417c */
    int32_t (*pointer_poll)(void *ctx);                                  /* [004c6194]: the buttons */
    int32_t (*key)(void *ctx);                                           /* 00464bd0 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);           /* 00463ef0: eax, edx */
    void (*free)(void *ctx, int32_t bank);                               /* 00475260 (cdecl) */
    void *ctx;
} ScreenShowOps;

/* 00466f68: name's palette ("!pal", palette_put into a local one and the
 * screen's) and background ("bgnd") drawn on the screen picture and shown;
 * modes as above (the mode and music are shorts). SCREEN_SHOW_WAIT waits 300
 * ticks with music, 500 without, until space, Enter, Escape or the active
 * pointer's button skips it. With music the stream is fed before the
 * drawing and after it all, and serviced while waiting. Returns 1 when
 * skipped. */
int screen_show(const char *name, int32_t mode, int32_t music, int32_t screen, int32_t screen_shape,
                uint8_t screen_palette[PALETTE_PUT_BYTES], DialogPointer *p, const ScreenShowOps *ops);

#endif
