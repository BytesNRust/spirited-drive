/* The standings screen after a race (00461540). */
#ifndef TNFS_STANDINGS_H
#define TNFS_STANDINGS_H

#include "game/frontend/palette_put.h"
#include "game/frontend/yes_no_box.h"

#include <stdint.h>

/* The calls 00461540 makes. Shapes are 004732dd's; a palette argument is
 * one of the screen's 0x300-byte palettes. */
typedef struct StandingsOps {
    void (*flush)(void *ctx);                                  /* 00464a30 */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);  /* 00464038: eax, edx (main's pointer_move_to) */
    int32_t (*load_art)(void *ctx, const char *name);          /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);      /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                             /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);  /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 004763cc (cdecl) */
    void (*palette_write)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*inverse_build)(void *ctx);                          /* 0047463c */
    void (*text_styles)(void *ctx);                            /* 0046376c */
    void (*pointer_poll)(void *ctx);                           /* [004c6194] */
    void (*pointer_hide)(void *ctx);                           /* [004c618c] */
    void (*pointer_draw)(void *ctx);                           /* [004c6190] */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                /* 00473680 (cdecl) */
    void (*select_default)(void *ctx);                         /* 00473604 */
    void (*blit)(void *ctx, const uint8_t *shape);             /* 004737ec (cdecl): at its own place */
    void (*blit_masked)(void *ctx, const uint8_t *shape);      /* 00473ab4 (cdecl) */
    void (*blit_word)(void *ctx, int32_t shape);               /* 004737ec on a picture's +0x2c */
    void (*text_select)(void *ctx, int32_t style);             /* 004638c0 */
    void (*text_boxed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463934 */
    void (*time_text)(void *ctx, char *out, int32_t time);     /* 00466d5c: eax out, edx a time */
    void (*results_order)(void *ctx);                          /* 0045b6c8 (the race side's) */
    void (*lock)(void *ctx);                                   /* 00473578 */
    void (*unlock)(void *ctx);                                 /* 004733b0 */
    void (*flip)(void *ctx);                                   /* 004733ec */
    void (*pointer_show)(void *ctx);                           /* 00441798 */
    void (*pointer_mark)(void *ctx, DialogPointer *p);         /* 00441858: the marks = the pointer's low halves */
    int32_t (*pointer_buttons)(void *ctx);                     /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*fade_out)(void *ctx, int32_t first, int32_t count, int32_t ebx);        /* 00463ef0: ebx the bank */
    void (*delay)(void *ctx, int32_t time);                    /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                   /* 0046492c */
    int32_t (*ticks)(void *ctx);                               /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);              /* 00464088 */
    int32_t (*key)(void *ctx);                                 /* 00464bd0 */
    void (*pointer_click)(void *ctx);                          /* 00441814 */
    void (*pointer_off)(void *ctx);                            /* 00464060 */
    void (*music_service)(void *ctx);                          /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                     /* 0047336d */
    void (*free)(void *ctx, int32_t bank);                     /* 00475260 (cdecl) */
    void (*free_inverse)(void *ctx);                           /* 00474558 */
    void *ctx;
} StandingsOps;

enum { STANDINGS_GAME = 0x5bc };

typedef struct StandingsInputs {
    const uint8_t *game;        /* 0052ad44: the game state, STANDINGS_GAME bytes (tournament.h) */
    const int32_t *finish;      /* 0051dccc: 8 dwords, each one's low half a player 0..7 */
    const int16_t *car_points;  /* the shorts 0051ce24 + 0x1d2 x player */
    int track;                  /* the short 00524e85: 0..8 */
    int segment;                /* the short 00524e83 */
    int laps;                   /* the short 00524ea1 */
    int german;                 /* the short 0052b31c */
    int level;                  /* the short 0052ad40 */
    int unknown_50a5b6;         /* the short 0050a5b6: 1 ends the screen */
    int32_t screen;             /* 00524834 */
    int32_t screen_shape;       /* [00524834] + 0x2c */
} StandingsInputs;

typedef struct StandingsState {
    int32_t inverse;                       /* 004c5ff4 */
    uint8_t *screen_palette;               /* 00520dd8 (palette_put) */
} StandingsState;

/* 00461540: segment set, one segment's standings (stats/sgmtstnd.qfs): the
 * track, "SEGMENT n" (laps "n Lap race" past the first three tracks), each
 * finisher's time (0 when flagged) and, on the first three, the second
 * time, and name; else the totals (finstnd.qfs when the tournament's nine
 * segments are run, or eight before the first unlock, else trmtstnd.qfs),
 * the players by points, each one's points and the car's (or "0"). The
 * palette is faded in, then the screen waits: the pointer over "Done" (0,
 * 0x1b3)..(0x82, 0x1e0) selects it, the arrows toggle it; Enter, space or
 * the left button on it (once armed) ends, as do Escape, the timer
 * (0047336d), the right button and 0050a5b6. */
void standings_screen(int segment_mode, const StandingsInputs *in, StandingsState *st, DialogPointer *p,
                      const StandingsOps *ops);

#endif
