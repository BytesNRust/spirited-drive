/* The showcase's performance and engine pages (00406008). */
#ifndef TNFS_SHOWCASE_FIGURES_H
#define TNFS_SHOWCASE_FIGURES_H

#include <stdint.h>

enum { SHOWCASE_FIGURES_PALETTE = 0x300, SHOWCASE_FIGURES_NAME = 0x28 };

typedef struct ShowcaseFiguresOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_bank)(void *ctx, const char *path, int32_t flags);        /* 00476268 (cdecl) */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194] */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*blit_masked)(void *ctx, const uint8_t *shape);                    /* 00473ab4 (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*blit_word)(void *ctx, int32_t shape);                             /* 004737ec with a picture's shape */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*voice_start)(void *ctx, const char *name);                        /* 00464788: eax (edx 0x100, unused) */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*voice_play)(void *ctx);                                           /* 00464850 */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    void (*car_flags)(void *ctx);                                            /* 00465ec8 (race_settings_car_flags) */
    void (*voice_stop)(void *ctx);                                           /* 0046486c */
    void (*voice_free)(void *ctx);                                           /* 004648a8 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void *ctx;
} ShowcaseFiguresOps;

/* config.dat's shorts and the globals the pages read, through pointers as the original reads memory */
typedef struct ShowcaseFiguresState {
    int16_t *player_car;            /* 00524e8b: 0..7, stepped by Next / Previous */
    const int16_t *race_type;       /* 00524e5d: 3 a tournament (its cars by track) */
    const int16_t *track;           /* 00524e85 */
    const int16_t *german;          /* the short 0052b31c: "per" keeps to two pages */
    const char *car_dir;            /* 00524738: the showcase's directory */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;               /* the short 00524792 */
    const int32_t *screen;          /* 00524834: the screen's picture */
} ShowcaseFiguresState;

/* 00406008 (eax which, "per" or "mec"; all registers kept): the car's
 * <car_dir><car><which>.qfs and its voice <car><which>16.eas, played with
 * each fade in, over one block of three pages ("per": acc1 / accl, bra1 /
 * brak, han1 / hand; "mec": engn, driv, chas; in German "per" toggles
 * between its first two), paged by the block's arrows (the pointer's, or
 * Left / Right while it is selected), and Done, Previous and Next as the
 * history pages (00407290). Returns 1. */
int32_t showcase_figures(ShowcaseFiguresState *st, const ShowcaseFiguresOps *ops, const char *which);

#endif
