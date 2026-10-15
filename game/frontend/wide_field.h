/* The wide text field: a line of text typed in a box 0x280 wide (00461f90). */
#ifndef TNFS_WIDE_FIELD_H
#define TNFS_WIDE_FIELD_H

#include <stdint.h>

/* the original's buffer (00461e90's 0x32 zeros on its stack) and how much is copied in and back */
enum { WIDE_FIELD_TYPED = 0x32, WIDE_FIELD_KEEP = 0x29 };

typedef struct WideFieldOps {
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
    void (*text_select)(void *ctx, int32_t style);                 /* 004638c0 */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8: eax, edx, ebx */
    int32_t (*keys_pending)(void *ctx);                            /* 00464bd0 */
    void (*music)(void *ctx);                                      /* 0046417c */
    void (*drain)(void *ctx);                                      /* 004770ac */
    int32_t (*pointer_poll)(void *ctx);                            /* [004c6194]: the buttons */
    void (*joystick_poll)(void *ctx);                              /* 0043d4c0: sets the state's joystick_key */
    int32_t (*key_read)(void *ctx);                                /* 00476f94: 0 when none */
    int32_t (*ticks)(void *ctx);                                   /* 00473334 */
    void (*picture_free)(void *ctx, int32_t picture);              /* 004746a0 (cdecl) */
    void (*layout_on)(void *ctx);                                  /* 004770a0, German only */
    void (*layout_off)(void *ctx);                                 /* 0047706c, German only */
    void *ctx;
} WideFieldOps;

typedef struct WideFieldState {
    int16_t pointer_active;  /* the short 00524790 */
    int16_t buttons;         /* the short 00524792: [004c6194]'s */
    int32_t joystick_key;    /* 004fce50: 0043d4c0's, not 0 while a joystick button is held */
} WideFieldState;

/* 00461f90 (eax text, edx max, ebx x, ecx y, the short on the stack
 * free_text; the language the short 0052b31c): the keys and buttons
 * settled, a box 0x280 wide centred on x, 0x23 high, at y, the screen under
 * it kept; the text copied (WIDE_FIELD_KEEP bytes, strncpy) and shown
 * centred with a '_' blinking every 15 ticks (summed as a short). The left
 * button or the joystick's is Enter, the right Escape. Characters 0x20..0x7e
 * (and in German ü ä ö ß µ Ä Ü Ö ° §) are added below max characters and
 * 0x262 pixels; space . \ / only with free_text; Backspace drops the last,
 * Delete all; in German a space or Backspace reads one more key and drops it.
 * Enter copies WIDE_FIELD_KEEP bytes back (the last 0) and gives 1, Escape
 * gives 0. The text needs WIDE_FIELD_KEEP bytes, its length below
 * WIDE_FIELD_TYPED, and max below WIDE_FIELD_KEEP. */
int32_t wide_field_edit(char *text, int32_t max, int32_t x, int32_t y, int32_t free_text, int german,
                        WideFieldState *st, const WideFieldOps *ops);

#endif
