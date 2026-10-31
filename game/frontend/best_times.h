/* The best times screen of the track screen (004676d8). */
#ifndef TNFS_BEST_TIMES_H
#define TNFS_BEST_TIMES_H

#include "game/frontend/track_select.h"

#include <stdint.h>

/* The calls 004676d8 makes; a palette argument is one of its 0x300-byte palettes. */
typedef struct BestTimesOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);  /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);          /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);      /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                             /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);  /* 004732dd (cdecl) */
    void (*flush)(void *ctx);                                  /* 00464a30 */
    void (*palette_set)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 004763cc (cdecl) */
    void (*inverse_build)(void *ctx);                          /* 0047463c */
    void (*text_styles)(void *ctx);                            /* 0046376c */
    void (*pointer_poll)(void *ctx);                           /* [004c6194] */
    void (*pointer_mark)(void *ctx, DialogPointer *p);         /* 00441858: the marks = the pointer's low halves */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                /* 00473680 (cdecl) */
    void (*blit)(void *ctx, const uint8_t *shape);             /* 004737ec (cdecl): at its own place */
    void (*blit_masked)(void *ctx, const uint8_t *shape);      /* 00473ab4 (cdecl) */
    void (*blit_word)(void *ctx, int32_t shape);               /* 004737ec on a picture's +0x2c */
    void (*text_select)(void *ctx, int32_t style);             /* 004638c0: eax */
    void (*text_fit)(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);  /* 00463d34 */
    void (*time_text)(void *ctx, char *out, int32_t time);     /* 00466d5c: eax out, edx the time */
    void (*speed_text)(void *ctx, char *out, int32_t speed);   /* 00466c9c: eax out, edx the speed */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8 */
    void (*select_default)(void *ctx);                         /* 00473604 */
    void (*pointer_hide)(void *ctx);                           /* [004c618c] */
    void (*lock)(void *ctx);                                   /* 00473578 */
    void (*unlock)(void *ctx);                                 /* 004733b0 */
    void (*flip)(void *ctx);                                   /* 004733ec */
    void (*pointer_show)(void *ctx);                           /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                     /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*deadline_set)(void *ctx, int32_t time);             /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                   /* 0046492c: eax */
    int32_t (*ticks)(void *ctx);                               /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);              /* 00464088 */
    int32_t (*key)(void *ctx);                                 /* 00464bd0 */
    void (*pointer_click)(void *ctx);                          /* 00441814 */
    void (*car_flags)(void *ctx);                              /* 00465ec8 (race_settings_car_flags) */
    void (*fade_out)(void *ctx, int32_t first, int32_t count); /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                           /* 00474558 */
    void (*free)(void *ctx, int32_t bank);                     /* 00475260 (cdecl) */
    int32_t (*load_bank)(void *ctx, const char *path, int32_t flags);  /* 00476268 (cdecl): the bank */
    void (*pointer_off)(void *ctx);                            /* 00464060 */
    void (*music)(void *ctx);                                  /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                     /* 0047336d */
    void (*music_fade_out)(void *ctx);                         /* 004644f0 */
    void (*music_end)(void *ctx);                              /* 00464568 */
    void *ctx;
    /* port: NULL unless the state has a `plus` */
    int32_t (*plus_toggle)(void *ctx);   /* 1 once a Tab (or the like) went down: the other table */
    void (*plus_text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);  /* the cheats item: 1 lit, 0 idle */
    int32_t (*plus_text_width)(void *ctx, const char *text);
} BestTimesOps;

enum {
    /* the records past the config image (00524ea5 + 0xa6b x track): ten of 0x27 bytes per laps choice (0x186
     * each, only past the first three tracks) from +0xea, then the top speed's from +0x57c; offsets here from
     * 00524f8f, the first record's name */
    BEST_TIMES_TRACK = 0xa6b,
    BEST_TIMES_CHOICE = 0x186,
    BEST_TIMES_ROW = 0x27,
    BEST_TIMES_ROWS = 10,
    BEST_TIMES_CAR = 0xf,         /* a dword: the car, 0..7 */
    BEST_TIMES_TIME = 0x1b,       /* a dword: 00466d5c's ticks */
    BEST_TIMES_SHIFT = 0x23,      /* a dword: 0 automatic, 1 manual (others nothing) */
    BEST_TIMES_TOP = 0x492,       /* the top speed's name ... */
    BEST_TIMES_TOP_CAR = 0x4a1,
    BEST_TIMES_TOP_SET = 0x4a9,   /* a dword: drawn when not 0 */
    BEST_TIMES_TOP_SPEED = 0x4b1, /* a dword: 00466c9c's speed */
    BEST_TIMES_TOP_SHIFT = 0x4b5,
    BEST_TIMES_RECORDS = 8 * BEST_TIMES_TRACK + BEST_TIMES_TOP_SHIFT + 4
};

typedef struct BestTimesInputs {
    const char *paths;         /* 00524328: the data path the reloaded qfs names start with */
    int german;                /* the short 0052b31c */
    int32_t lost_vegas;        /* 004baf18 */
    int32_t screen;            /* 00524834 */
    int32_t screen_shape;      /* [00524834] + 0x2c */
    const uint8_t *records;    /* 00524f8f, BEST_TIMES_RECORDS bytes */
} BestTimesInputs;

/* port: the two tables a track's records can be shown as (records_route.h): the one without cheats and the one with, each
 * laid out as the records at 00524f8f are (BEST_TIMES_RECORDS bytes, nine tracks 0xa6b apart), and the marks of the one
 * with cheats (a byte per slot of records_route.h, a table per track) */
typedef struct BestTimesPlus {
    const uint8_t *records[2];
    const uint8_t *marks[9];
    int cheats;   /* the table shown now: 0 or 1 */
    /* port (free records): 1 puts a lap-count item between the arrows on a circuit (free_records.h's tables, for the record variant) */
    int laps_select;
    int variant;  /* RecordVariant of the tables shown */
} BestTimesPlus;

typedef struct BestTimesState {
    uint8_t *config;           /* 00524df9, TRACK_SELECT_CONFIG bytes (track_select.h's offsets) */
    int32_t inverse;           /* 004c5ff4 */
    uint8_t *screen_palette;   /* 00520dd8 (palette_put) */
    BestTimesPlus *plus;       /* port: NULL is the original screen exactly */
} BestTimesState;

/* 004676d8: tracksel/<track>bst.qfs and .inv: the ten best times of the
 * track (by laps choice past the first three) and its top speed, each with
 * its car and gearbox. The pointer or the arrows pick Done, the track before
 * or after (a tournament takes its car; the laps from the choice; the art
 * loaded again under the data path) or back; Enter, space or the left
 * button (once armed) act; Escape, the timer and the right button go back.
 * Returns 1 for Done (the track screen ends too), else 0.
 * port: with a `plus`, Tab, Up, Down (and the right button, which no longer goes back) show the other table, and a
 * footer item between Done and the left arrow (Right from Done, Left from the arrow, the pointer, Enter) does the same. */
int32_t best_times_screen(const BestTimesInputs *in, BestTimesState *st, DialogPointer *p, const BestTimesOps *ops);

#endif
