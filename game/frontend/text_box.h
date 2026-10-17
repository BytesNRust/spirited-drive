/* A shadowed text put on the screen through a picture of its own. */
#ifndef TNFS_TEXT_BOX_H
#define TNFS_TEXT_BOX_H

#include <stdint.h>

/* The calls 00463934 makes; pictures are EA's (0047310c's), passed as words. */
typedef struct TextBoxOps {
    int32_t (*text_width)(void *ctx, const char *text);                      /* 004770f4 (cdecl) */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*select)(void *ctx, int32_t picture);                    /* 00473680 (cdecl): the draw surface */
    void (*grab)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 00476e28 (cdecl): the screen into shape */
    void (*ramp)(void *ctx, int shadow);   /* 0048ec60: the shadow's ramp (0051fd98) or the current (0051dd18) */
    void (*draw)(void *ctx, const char *text, int32_t x, int32_t y);  /* 0048c264 (cdecl) */
    void (*pointer_hide)(void *ctx);                               /* [004c618c] */
    void (*blit)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 0047380c (cdecl) */
    void (*select_default)(void *ctx);                             /* 00473604 */
    void (*blit_own)(void *ctx, int32_t shape);                    /* 004737ec (cdecl): at its own place */
    void (*flip)(void *ctx);                                       /* 004733ec */
    void (*picture_free)(void *ctx, int32_t picture);              /* 004746a0 (cdecl) */
    void *ctx;
} TextBoxOps;

/* 00463934: a 0x2d-high picture as wide as the text plus 10 takes the
 * screen under x, y, gets the text with its shadow (at 2, 2 and 0, 0), and
 * goes back to x, y with the pointer hidden; the whole screen picture is
 * blitted at its own place, flipped, and the picture freed. */
void text_box_show(const char *text, int32_t x, int32_t y, int32_t screen, const TextBoxOps *ops);

/* 00463a18: the text cut as 00463d34 cuts it (the first n characters, n
 * from 7, stopping at the first whose width passes `width`, or at 49), then
 * put on the screen as text_box_show puts it. */
void text_box_fit_show(const char *text, int32_t x, int32_t y, int32_t width, int32_t screen,
                       const TextBoxOps *ops);

#endif
