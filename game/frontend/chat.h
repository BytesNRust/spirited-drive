/* The link game's chat screen (0040a5d8), Control Central's chat op. */
#ifndef TNFS_CHAT_H
#define TNFS_CHAT_H

#include <stdint.h>

/* the message it sends (its stack at +0x300): the kind (6) as a short, the
 * player's name at +2, two typed lines of 0x6a bytes at +0xb and +0x75 */
enum { CHAT_MESSAGE = 0xdf, CHAT_NAME = 2, CHAT_TEXT = 0xb, CHAT_LINE = 0x6a, CHAT_KIND = 6,
       CHAT_HISTORY = 9, CHAT_HISTORY_LINE = 0x277, CHAT_PALETTE = 0x300 };

typedef struct ChatOps {
    int32_t (*load_bank)(void *ctx, const char *path);                       /* 00466c34: eax */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*blit_masked)(void *ctx, const uint8_t *shape);                    /* 00473ab4 (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*blit_word)(void *ctx, int32_t shape);                             /* 004737ec with a picture's shape */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    int32_t (*pointer_poll)(void *ctx);                                      /* [004c6194]: the buttons */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*inverse_build)(void *ctx);                                        /* 0047463c */
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    void (*grab)(void *ctx, int32_t shape, int32_t x, int32_t y);            /* 00476e28 (cdecl): the surface into shape */
    void (*drain)(void *ctx);                                                /* 004770ac */
    int32_t (*keys_pending)(void *ctx);                                      /* 00464bd0 */
    void (*music)(void *ctx);                                                /* 0046417c */
    void (*layout_on)(void *ctx);                                            /* 004770a0, German only */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    void (*link_service)(void *ctx);                                         /* 004104d0: the link's messages */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    int32_t (*lines_ready)(void *ctx, uint32_t lock);                        /* 00476c9c (cdecl): the lock taken */
    void (*text_select)(void *ctx, int32_t style);                           /* 004638c0 */
    void (*blit)(void *ctx, int32_t shape, int32_t x, int32_t y);            /* 004737d0 (cdecl) */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8: eax, edx, ebx */
    void (*show)(void *ctx, int32_t shape, int32_t x, int32_t y);            /* 0047380c (cdecl) */
    void (*yield)(void *ctx, int32_t ms);                                    /* 00475c14 (cdecl) */
    void (*lines_done)(void *ctx, uint32_t lock);                            /* 00476cfc (cdecl): the lock given back */
    int32_t (*key_read)(void *ctx);                                          /* 00476f94: 0 when none */
    int32_t (*joystick_poll)(void *ctx);                                     /* 0043d4c0: sets 004fce50 / 004fce4c */
    int32_t (*text_width)(void *ctx, const char *text);                      /* 004770f4 (cdecl) */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*send)(void *ctx, int32_t link, const uint8_t *message, int32_t size);     /* 00445e00: eax, edx, ebx */
    void (*on_chat)(void *ctx, int32_t link, const uint8_t *message, int32_t size);  /* 0040a3f0: eax, edx, ebx */
    void (*surface_lock)(void *ctx);                                         /* 004735dc */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*picture_free)(void *ctx, int32_t picture);                        /* 004746a0 (cdecl) */
    void (*layout_off)(void *ctx);                                           /* 0047706c, German only */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*art_free)(void *ctx);                                             /* 00474558 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void *ctx;
} ChatOps;

/* the globals it reads and writes, through pointers as the original reads memory */
typedef struct ChatState {
    const char *name;               /* 00524df0: config.dat's player name */
    const int16_t *host;            /* 00524e7b: this machine hosts the link game */
    const uint8_t *lines;           /* 004b7a30: the lines come in, CHAT_HISTORY x CHAT_HISTORY_LINE */
    const int16_t *line;            /* 004b905f: the next line 0040a3f0 writes */
    int16_t *arrived;               /* 004b9062: a line came in */
    const uint32_t *lines_lock;     /* 004f824c */
    const int32_t *joystick_key;    /* 004fce50: 0043d4c0's key */
    const int32_t *joystick_key2;   /* 004fce4c */
    const int16_t *german;          /* 0052b31c */
    const int16_t *link_lost;       /* 0052b314 */
    const int16_t *game_over;       /* 0050a5b6: 1 leaves */
    int16_t *up;                    /* 0052b318: set while the screen is up */
    int16_t *central_redraw;        /* 0052b31a: set on leaving, Central draws itself again */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;               /* the short 00524792 */
    const int32_t *screen;          /* 00524834: the screen's picture */
} ChatState;

/* 0040a5d8 (no arguments; all registers kept): control/chat.qfs; the lines
 * come in (0040a3f0's, nine from 004b905f round) drawn when 004b9062 is set
 * and 00476c9c takes the lock; two lines typed with a blinking cursor,
 * wrapped at 0x20b pixels (a last word under 7 characters taken down with
 * it), joined back by Backspace / Delete; Enter sends the name and the
 * lines (00445e00, and to this machine's own lines through 0040a3f0 when it
 * hosts). Done (the pointer, Enter or the left button on it), Escape, the
 * right button, 0052b314 or 0050a5b6 == 1 leave. Port guard: the cursor and
 * the name copy are kept within the message (the original writes past it
 * into its stack), and a 004b905f outside 0..9 draws no line text. */
void chat_screen(ChatState *st, const ChatOps *ops);

#endif
