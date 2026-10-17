/* The number field: a phone number typed in a box (004632b8). */
#ifndef TNFS_NUMBER_FIELD_H
#define TNFS_NUMBER_FIELD_H

#include <stdint.h>

/* the original's buffer (00461f00's 0x16 zeros on its stack) and how much Enter copies back */
enum { NUMBER_FIELD_TYPED = 0x16, NUMBER_FIELD_KEEP = 0x14 };

typedef struct NumberFieldOps {
    int32_t (*text_width)(void *ctx, const char *text);                      /* 004770f4 (cdecl) */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*select)(void *ctx, int32_t picture);                    /* 00473680 (cdecl): the draw surface */
    void (*select_default)(void *ctx);                             /* 00473604 */
    void (*pointer_hide)(void *ctx);                               /* [004c618c] */
    void (*grab)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 00476e28 (cdecl): the screen into shape */
    void (*unlock)(void *ctx);                                     /* 004733b0 */
    void (*flip)(void *ctx);                                       /* 004733ec */
    void (*blit)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 004737d0 (cdecl) */
    void (*show)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 0047380c (cdecl) */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8: eax, edx, ebx */
    int32_t (*keys_pending)(void *ctx);                            /* 00464bd0 */
    void (*music)(void *ctx);                                      /* 0046417c */
    void (*drain)(void *ctx);                                      /* 004770ac */
    void (*pointer_poll)(void *ctx);                               /* [004c6194] */
    int32_t (*key_read)(void *ctx);                                /* 00476f94: 0 when none */
    int32_t (*ticks)(void *ctx);                                   /* 00473334 */
    void (*picture_free)(void *ctx, int32_t picture);              /* 004746a0 (cdecl) */
    void *ctx;
} NumberFieldOps;

/* 004632b8 (from 00451b8c; eax text, dx max, bx x, cx y, all but text as
 * shorts): a box 30 x (max / 2) + 5 either side of x, 0x23 high, at y; the
 * screen under it kept. The text is copied (sprintf'd with itself as the
 * format, so it must have no '%') and shown centred, with a '_' blinking
 * every 15 ticks (summed as a short) while no key comes. Digits and
 * - ( ) + / , # ! are added below max characters; Delete or Backspace
 * drops the last; in German a space or Backspace reads one more key and
 * drops it. Enter copies NUMBER_FIELD_KEEP bytes back (strncpy) and gives
 * 1; Escape gives 0; both put the screen back and free the two pictures.
 * The text needs NUMBER_FIELD_KEEP bytes, and both its length and max
 * must be below NUMBER_FIELD_TYPED. */
int32_t number_field_edit(char *text, int32_t max, int32_t x, int32_t y, int german, const NumberFieldOps *ops);

#endif
