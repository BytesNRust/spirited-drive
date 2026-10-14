/* The track screen of Control Central (004687d0). */
#ifndef TNFS_TRACK_SELECT_H
#define TNFS_TRACK_SELECT_H

#include "game/frontend/palette_put.h"
#include "game/frontend/yes_no_box.h"

#include <stdint.h>

/* The calls 004687d0 makes. Shapes are 004732dd's; a palette argument is
 * one of the screen's 0x300-byte palettes. */
typedef struct TrackSelectOps {
    void (*flush)(void *ctx);                                  /* 00464a30 */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);  /* 00464038: eax, edx */
    int32_t (*load_bank)(void *ctx, const char *path, int32_t flags);  /* 00476268 (cdecl): the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);      /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                             /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);  /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 004763cc (cdecl) */
    void (*inverse_build)(void *ctx);                          /* 0047463c */
    void (*text_styles)(void *ctx);                            /* 0046376c */
    void (*pointer_poll)(void *ctx);                           /* [004c6194] */
    void (*pointer_hide)(void *ctx);                           /* [004c618c] */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                /* 00473680 (cdecl) */
    void (*select_default)(void *ctx);                         /* 00473604 */
    void (*blit)(void *ctx, const uint8_t *shape);             /* 004737ec (cdecl): at its own place */
    void (*blit_masked)(void *ctx, const uint8_t *shape);      /* 00473ab4 (cdecl) */
    void (*blit_word)(void *ctx, int32_t shape);               /* 004737ec on a picture's +0x2c */
    void (*text_select)(void *ctx, int32_t style);             /* 004638c0 */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8 */
    void (*lock)(void *ctx);                                   /* 00473578 */
    void (*unlock)(void *ctx);                                 /* 004733b0 */
    void (*flip)(void *ctx);                                   /* 004733ec */
    void (*pointer_show)(void *ctx);                           /* 00441798 */
    void (*pointer_mark)(void *ctx, DialogPointer *p);         /* 00441858: the marks = the pointer's low halves */
    int32_t (*pointer_buttons)(void *ctx);                     /* 004417f8 */
    void (*music_start)(void *ctx, const char *name);          /* 00464788: eax the music's file */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*music_play)(void *ctx);                             /* 00464850 (ebx 0 to EA's 004925a8) */
    void (*delay)(void *ctx, int32_t time);                    /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                   /* 0046492c */
    int32_t (*ticks)(void *ctx);                               /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);              /* 00464088 */
    int32_t (*key)(void *ctx);                                 /* 00464bd0 */
    void (*pointer_click)(void *ctx);                          /* 00441814 */
    void (*music_stop)(void *ctx);                             /* 0046486c */
    void (*music_free)(void *ctx);                             /* 004648a8 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count); /* 00463ef0: eax, edx */
    void (*free)(void *ctx, int32_t bank);                     /* 00475260 (cdecl) */
    void (*free_inverse)(void *ctx);                           /* 00474558 */
    int32_t (*best_times)(void *ctx);                          /* 004676d8: its screen; non-zero ends this one */
    void (*car_flags)(void *ctx);                              /* 00465ec8 (race_settings_car_flags) */
    void (*pointer_off)(void *ctx);                            /* 00464060 */
    void (*music_service)(void *ctx);                          /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                     /* 0047336d */
    void (*music_fade_out)(void *ctx);                         /* 004644f0 */
    void (*music_end)(void *ctx);                              /* 00464568 */
    void *ctx;
} TrackSelectOps;

enum {
    /* the config image 00524df9 (0xac bytes; Escape puts it back as it was) and the shorts in it */
    TRACK_SELECT_CONFIG = 0xac,
    TRACK_SELECT_RACE_TYPE = 0x64,   /* 00524e5d */
    TRACK_SELECT_OPPONENT = 0x88,    /* 00524e81 */
    TRACK_SELECT_TRACK = 0x8c,       /* 00524e85 */
    TRACK_SELECT_CAR = 0x92,         /* 00524e8b */
    TRACK_SELECT_LAPS = 0xa8,        /* 00524ea1 */
    TRACK_SELECT_LAPS_CHOICE = 0xaa  /* 00524ea3: 0, 1, 2 the short, middle, long race (RaceSettings.difficulty) */
};

typedef struct TrackSelectInputs {
    const char *paths;   /* 00524328: the data path the qfs names start with */
    int german;          /* the short 0052b31c */
    int32_t variant_m;   /* 004fbad8 */
    int32_t variant_r;   /* 004fbadc */
    int32_t lost_vegas;  /* 004baf18 */
    int32_t screen;      /* 00524834 */
    int32_t screen_shape;  /* [00524834] + 0x2c */
} TrackSelectInputs;

typedef struct TrackSelectState {
    uint8_t *config;          /* 00524df9, TRACK_SELECT_CONFIG bytes */
    int32_t *time_of_day;     /* 004fbae0: a dword per track, 0 morning, 1 midday, 2 evening */
    int32_t inverse;          /* 004c5ff4 */
    uint8_t *screen_palette;  /* 00520dd8 (palette_put) */
} TrackSelectState;

/* 004687d0: the track screen (tracksel/<track>sel.qfs and .inv): the
 * track's art, its time of day (not on Lost Vegas or with a variant), its
 * length (miles, or km in German; on the first three tracks a segment's,
 * picked by "ssg1".."ssg3") and the laps past them. The pointer or the
 * arrows pick one of Done, Prev, Next, Best, the three segments, the laps
 * and the time of day; Enter, space or the left button (once armed) act on
 * it: the track steps back or on (a tournament takes its car), the laps
 * choice steps (not in a tournament), the time of day toggles, Best runs
 * 004676d8. Escape restores the config image; Done, the timer and the
 * right button end. Returns 1. */
int track_select_screen(const TrackSelectInputs *in, TrackSelectState *st, DialogPointer *p,
                        const TrackSelectOps *ops);

#endif
