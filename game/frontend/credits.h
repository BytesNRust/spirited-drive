/* The credits screen: credits.fsh's five pages (004166e0). */
#ifndef TNFS_CREDITS_H
#define TNFS_CREDITS_H

#include <stdint.h>

enum { CREDITS_PALETTE = 0x300 }; /* 004166e0's palette on its stack */

typedef struct CreditsOps {
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*blit_at)(void *ctx, int32_t shape, int32_t x, int32_t y);         /* 004737d0 (cdecl) */
    void (*unlock)(void *ctx);                                               /* 004733b0 */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*pointer_read)(void *ctx);                                      /* [004c6194]: the buttons */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*music_fade_out)(void *ctx);                                       /* 004644f0 */
    void (*music_end)(void *ctx);                                            /* 00464568 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    void *ctx;
    /* port, after ctx so the original's initializers leave it NULL: our own pages after the original's five (not when the player left early); the music plays on */
    void (*extra)(void *ctx);
} CreditsOps;

/* the globals the screen reads, through pointers as the original reads memory */
typedef struct CreditsState {
    const int32_t *screen;          /* 00524834: the screen's picture */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;               /* the short 00524792 */
    const int16_t *quit;            /* the short 0050a5b6: 1 leaves */
    int32_t *idle;                  /* 004bb8dc: counted up by the timer (0043b9e8), 100 a second */
} CreditsState;

/* 004166e0 (all registers kept; options passes eax 0, unused): credits.fsh's
 * pages bgn1..bgn5 drawn and faded in one after another; Space, Enter, the
 * left button or 2000 ticks of 004bb8dc go on to the next, Escape, the right
 * button or 0050a5b6 == 1 leave. The buttons count only once they have been
 * seen up, and stay counted for the pages after. */
void credits_show(const CreditsState *st, const CreditsOps *ops);

#endif
