/* The race's results screen (0045cea0), shown after a race before the standings. */
#ifndef TNFS_RESULTS_SCREEN_H
#define TNFS_RESULTS_SCREEN_H

#include "game/frontend/palette_put.h"
#include "game/frontend/yes_no_box.h"

#include <stdint.h>

/* The calls 0045cea0 makes. Shapes are 004732dd's; a palette argument is
 * one of the screen's 0x300-byte palettes. Text goes out through a 0x5c
 * byte buffer (the original's on its stack). */
typedef struct ResultsOps {
    int32_t (*records)(void *ctx);                             /* 0045bad8: the records beaten, 1 / 2 / 3 */
    void (*flush)(void *ctx);                                  /* 00464a30 */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);  /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);          /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);      /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                             /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);  /* 004732dd (cdecl) */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*text_styles)(void *ctx);                            /* 0046376c */
    void (*pointer_poll)(void *ctx);                           /* [004c6194] */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                /* 00473680 (cdecl) */
    void (*blit)(void *ctx, const uint8_t *shape);             /* 004737ec (cdecl): at its own place */
    void (*text_select)(void *ctx, int32_t style);             /* 004638c0 */
    void (*text_at)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8 */
    void (*text_fit)(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);  /* 00463d34 */
    void (*time_text)(void *ctx, char *out, int32_t time);     /* 00466d5c: eax out, edx a time */
    void (*average_text)(void *ctx, char *out, int32_t time, int32_t distance);  /* 00466e5c: eax, edx, ebx */
    void (*speed_text)(void *ctx, char *out, int32_t speed);   /* 00466c9c: eax out, edx the speed */
    void (*palette_write)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*pointer_draw)(void *ctx);                           /* [004c6190] */
    void (*select_default)(void *ctx);                         /* 00473604 */
    void (*pointer_hide)(void *ctx);                           /* [004c618c] */
    void (*lock)(void *ctx);                                   /* 00473578 */
    void (*blit_word)(void *ctx, int32_t shape);               /* 004737ec on a picture's +0x2c */
    void (*unlock)(void *ctx);                                 /* 004733b0 */
    void (*flip)(void *ctx);                                   /* 004733ec */
    void (*pointer_show)(void *ctx);                           /* 00441798 */
    void (*pointer_mark)(void *ctx, DialogPointer *p);         /* 00441858: the marks = the pointer's low halves */
    int32_t (*pointer_buttons)(void *ctx);                     /* 004417f8 */
    void (*track_music)(void *ctx, const char *name);          /* 00464788: eax the name */
    void (*track_play)(void *ctx);                             /* 00464850 */
    void (*delay)(void *ctx, int32_t time);                    /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                   /* 0046492c */
    int32_t (*ticks)(void *ctx);                               /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);              /* 00464088 */
    int32_t (*key)(void *ctx);                                 /* 00464bd0 */
    void (*pointer_click)(void *ctx);                          /* 00441814 */
    void (*pointer_off)(void *ctx);                            /* 00464060 */
    void (*music_service)(void *ctx);                          /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                     /* 0047336d */
    void (*track_stop)(void *ctx);                             /* 0046486c */
    void (*music_free)(void *ctx);                             /* 004648a8 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count); /* 00463ef0: eax, edx */
    void (*free)(void *ctx, int32_t bank);                     /* 00475260 (cdecl) */
    void (*free_inverse)(void *ctx);                           /* 00474558 */
    void (*blit_masked)(void *ctx, const uint8_t *shape);      /* 00473ab4 (cdecl): 0045c45c's */
    int32_t (*totals)(void *ctx);                              /* 0045be50: 0045de08's, the row or 0x14 (ax) */
    int32_t (*top_record)(void *ctx);                          /* 0045c2d0: 1 when beaten (ax) */
    int32_t (*leads)(void *ctx);                               /* 0045b73c: the player leads the tournament */
    void (*best_totals)(void *ctx, int32_t row, int32_t top);  /* 0045c45c: eax, edx */
    int32_t (*lap_record)(void *ctx);                          /* 0045b908: 0045eff4's, 1 when beaten (ax) */
    void (*results_order)(void *ctx);                          /* 0045b6c8: the finishers (finish) by total */
    int32_t (*lap_times)(void *ctx, int32_t record, int32_t tune);  /* 0046067c: eax, edx; ax */
    void *ctx;
} ResultsOps;

enum { RESULTS_GAME = 0x5bc, RESULTS_IMAGE = 0x5f34, RESULTS_CARS = 12 };

typedef struct ResultsInputs {
    const uint8_t *game;        /* 0052ad44: the game state, RESULTS_GAME bytes (tournament.h) */
    const uint8_t *image;       /* 00524df0: config.dat's image, RESULTS_IMAGE bytes (the race setup, the records) */
    const int16_t *dropped;     /* 004f82a0: a short per player, set when it did not finish */
    int32_t player;             /* 004fbb08: this machine's player */
    int place;                  /* the short 0051dcc2: the player's place, from 0 */
    int rival_place;            /* the short 0051dcc0 */
    int rival;                  /* the short 0051dcc6: the rival player */
    int rival_car;              /* the short 0051dcc8: its car's short name */
    int german;                 /* the short 0052b31c: the words */
    int kmh;                    /* port: the distances' units (resource_kmh), which the language does not change */
    int unknown_50a5b6;         /* the short 0050a5b6: 1 ends the screen */
    int32_t screen;             /* 00524834 */
    int32_t screen_shape;       /* [00524834] + 0x2c */
    const char *const *car_names;  /* 004c5c84: RESULTS_CARS short names */
    int32_t lost_vegas;         /* 004baf18: 0045de08's, a ninth track counted */
    int32_t variant_r;          /* 004fbadc: 0046067c's, the track's "r" layout */
    int circuit_place;          /* the short 0051dcc4: 0045eff4's player's place, from 0 */
    int circuit_rival_place;    /* the short 0051dcbe: 0045eff4's rival's */
    const int32_t *finish;      /* 0051dccc: 8 dwords, the players by total (results_order fills it) */
} ResultsInputs;

typedef struct ResultsState {
    int32_t inverse;            /* 004c5ff4 */
    uint8_t *screen_palette;    /* 00520dd8 (palette_put) */
    int16_t record_music;       /* the short 0051dd14: a record's tune playing */
} ResultsState;

/* 0045cea0 (ax: ask 0045bad8 for the records beaten): stats/standseg.qfs
 * for the segment (00524e83, the last but one when 0) on the track: the
 * place ("%d / 8", "1 / 2" / "2 / 2" against the other player or the
 * segment's best, or DNF), the player's name, car, time, total (0 when
 * flagged), average and top speed against the rival's (the best time's on
 * a time trial), then the track's two records (the record lines lit when
 * beaten), each with its car and gearbox. A record beaten with the track
 * music on (00524e6b) plays a fanfare. The pointer over "Done" (0,
 * 0x1b3)..(0x82, 0x1e0) selects it, an arrow moves back onto it; Enter,
 * space or the left button on it (once armed) ends, as do Escape, the
 * timer (0047336d), the right button and 0050a5b6. */
void results_screen(int32_t ask_records, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                    const ResultsOps *ops);

#endif
