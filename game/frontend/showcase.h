/* The car showcase: a car's history, video, general, performance and engine pages (00408724). */
#ifndef TNFS_SHOWCASE_H
#define TNFS_SHOWCASE_H

#include <stdint.h>

enum { SHOWCASE_PALETTE = 0x300, SHOWCASE_NAME = 0x28, SHOWCASE_CARS = 8 };

typedef struct ShowcaseOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_bank)(void *ctx, const char *path, int32_t flags);        /* 00476268 (cdecl) */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    void (*flush)(void *ctx);                                                /* 00464a30 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*palette_write)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 004763cc (cdecl) */
    void (*inverse_build)(void *ctx);                                        /* 0047463c */
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194] */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*blit_masked)(void *ctx, const uint8_t *shape);                    /* 00473ab4 (cdecl) */
    void (*palette_show)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*blit_word)(void *ctx, int32_t shape);                             /* 004737ec with a picture's shape */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*pages)(void *ctx, const char *which);                             /* 00407290: eax "his" or "gen" */
    void (*figures)(void *ctx, const char *which);                           /* 00406008: eax "per" or "mec" */
    void (*music_fade_out)(void *ctx);                                       /* 004644f0 */
    void (*music_end)(void *ctx);                                            /* 00464568 */
    void (*movie)(void *ctx, const char *name);                              /* 004419c0: eax (intro.h intro_movie) */
    void (*music_start)(void *ctx, const char *name);                        /* 00464428: eax */
    void (*car_flags)(void *ctx);                                            /* 00465ec8 (race_settings_car_flags) */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void *ctx;
} ShowcaseOps;

/* port: the car variant box over the General photo (race/car_variant.h). The screen takes it as an item of its own: it
 * is lit when selected, Enter, the click and Left/Right step its choice. NULL leaves the screen as the game's. */
/* the box's place: under the heading (its words end at y 88), its bottom edge on the General photo's top (y 164) */
enum { VARIANT_LEFT = 136, VARIANT_RIGHT = 290, VARIANT_TOP = 100, VARIANT_BOTTOM = 163 };

typedef struct ShowcasePlus {
    int (*offered)(void *ctx, int car);                      /* 1 when the car has choices: the box is there */
    void (*step)(void *ctx, int car, int step);              /* the next (1) or previous (-1) choice */
    void (*free_colours)(void *ctx);                         /* the screen drawn with every item's art: notes the palette
                                                              * colours it leaves unused, which the box paints with */
    void (*draw)(void *ctx, int car, int lit, uint8_t *palette);  /* paints the box with the current choice, lit or dark;
                                                              * it may set the colours it took in `palette` (8 bits) */
    void *ctx;
} ShowcasePlus;

/* config.dat's shorts and the globals the screen reads, through pointers as the original reads memory */
typedef struct ShowcaseState {
    int16_t *player_car;            /* 00524e8b: 0..7, stepped by Next / Previous */
    const int16_t *race_type;       /* 00524e5d: 3 a tournament (its cars by track) */
    const int16_t *track;           /* 00524e85 */
    const int16_t *intro_movies;    /* the short 0052b2fc: the video offered */
    const char *car_dir;            /* 00524738: the showcase's directory */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;               /* the short 00524792 */
    const int32_t *screen;          /* 00524834: the screen's picture */
    const ShowcasePlus *plus;       /* port: the variant box, or NULL */
} ShowcaseState;

/* 00408724 (all registers kept): the car's <car_dir><car>.qfs (rx7, nsx,
 * supra, 911, vette, viper, 512tr, diablo by 00524e8b) over eight items:
 * history and general (00407290), the video (004419c0 with <car>.tgv, then
 * showcase.asf; only with 0052b2fc), performance and engine (00406008), Next
 * and Previous (the car stepped, within a tournament's cars for its track),
 * Done. The pointer's boxes pick, the arrows step by the item tables; Space,
 * Enter or the left button take the item once a key was seen up or 0x19
 * ticks passed; Escape, 0x2ee0 ticks idle or the right button leave.
 * Returns 1. */
int32_t showcase(ShowcaseState *st, const ShowcaseOps *ops);

#endif
