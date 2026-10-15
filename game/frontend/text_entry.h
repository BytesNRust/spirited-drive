/* The text field: a name typed in a box (004626d0). */
#ifndef TNFS_TEXT_ENTRY_H
#define TNFS_TEXT_ENTRY_H

#include <stdint.h>

/* the original's buffer (00461ec2's 0x2a zeros on its stack) */
enum { TEXT_ENTRY_TYPED = 0x2a };

typedef struct TextEntryOps {
    int32_t (*text_width)(void *ctx, const char *text);                      /* 004770f4 (cdecl) */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*select)(void *ctx, int32_t picture);                    /* 00473680 (cdecl): the draw surface */
    void (*select_default)(void *ctx);                             /* 00473604 */
    void (*pointer_hide)(void *ctx);                               /* [004c618c] */
    void (*grab)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 00476e28 (cdecl): the surface into shape */
    void (*flip)(void *ctx);                                       /* 004733ec */
    void (*blit)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 004737d0 (cdecl) */
    void (*blit_own)(void *ctx, int32_t shape);                    /* 004737ec (cdecl) */
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
} TextEntryOps;

typedef struct TextEntryState {
    int32_t screen;          /* 00524834: the screen's picture */
    int16_t pointer_active;  /* the short 00524790 */
    int16_t buttons;         /* the short 00524792: [004c6194]'s */
    int32_t joystick_key;    /* 004fce50: 0043d4c0's, not 0 while a joystick button is held */
} TextEntryState;

/* 004626d0 (eax text, edx max, ebx x, ecx y, all but text as shorts; the
 * short on the stack free_text; the language the short 0052b31c): as the
 * wide field (00461f90), but the box 30 x (max / 2) + 5 either side of x, as
 * the number field's; drawn through the screen's picture; the text at most
 * 0x1b8 pixels; % \ / | never taken, and without free_text neither
 * space . * + [ ] : ; " < > , ? and a..z made capitals. Enter is taken only
 * once the text has a letter or a digit (004920ac, ASCII); it copies max + 1
 * bytes back (strncpy) and gives 1, Escape gives 0. Delete clears the whole
 * buffer. The text and max must be below TEXT_ENTRY_TYPED. */
int32_t text_entry_edit(char *text, int32_t max, int32_t x, int32_t y, int32_t free_text, int german,
                        TextEntryState *st, const TextEntryOps *ops);

#endif
