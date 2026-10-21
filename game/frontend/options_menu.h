/* The options screen: transmission, ABS, traction control and the sub-screens (00444bb0). */
#ifndef TNFS_OPTIONS_MENU_H
#define TNFS_OPTIONS_MENU_H

#include <stdint.h>

enum { OPTIONS_MENU_PALETTE = 0x300 }; /* 00444bb0's palette on its stack */

typedef struct OptionsMenuOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);                    /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194] */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*unlock)(void *ctx);                                               /* 004733b0 */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*audio)(void *ctx, int32_t mode);                                  /* 004015dc: eax 0, the audio screen */
    void (*graphics)(void *ctx, int32_t mode);                               /* 00423d28: eax 0, the graphics screen */
    void (*controls)(void *ctx, int32_t mode);                               /* 00413cb0: eax 0, the controls screen */
    void (*load_save)(void *ctx, int32_t mode);                              /* 0042c498: eax 0 (load_save.h) */
    void (*music_fade_out)(void *ctx);                                       /* 004644f0 */
    void (*music_end)(void *ctx);                                            /* 00464568 */
    void (*music_start)(void *ctx, const char *name);                        /* 00464428: eax */
    void (*credits)(void *ctx, int32_t mode);                                /* 004166e0: eax 0 */
    void (*music_pick)(void *ctx);                                           /* 004562e8 */
    void (*car_flags)(void *ctx);                                            /* 00465ec8 (race_settings_car_flags) */
    void (*music_fade_to)(void *ctx, int32_t volume);                        /* 00464604: eax */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    void (*net_sync)(void *ctx);                                             /* 004104d0 (race_net_sync) */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    /* port: the Extras row's screen (extras_menu_run). Not in the original: it opens over the live Options picture
     * and Options carries on behind it. */
    void (*extras)(void *ctx);
    void (*plus_text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);  /* the game's text, lit or dark */
    int32_t (*plus_text_width)(void *ctx, const char *text);
    void (*plus_panel)(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       int32_t framed);
    void *ctx;
} OptionsMenuOps;

/* config.dat's shorts and the globals the screen reads; read through the pointers as the original reads memory,
 * since the sub-screens (a game loaded) change them */
typedef struct OptionsMenuState {
    int16_t *transmission;        /* 00524e87: 0 or 1 (the Aut / Man art), toggled */
    int16_t *automatic;           /* 00524e7d: set to transmission == 0 with each toggle */
    int16_t *abs;                 /* 00524e89 (race_settings.h car_flag_a): 0 or 1 toggled, 2 not offered */
    int16_t *traction;            /* 00524e7f (car_flag_b): the same */
    const int16_t *race_type;     /* 00524e5d: 3 a tournament */
    const int16_t *track;         /* 00524e85: 0..8 */
    int16_t *opponent;            /* 00524e81 */
    int16_t *player_car;          /* 00524e8b */
    int16_t *net_car;             /* 00524e9d */
    int16_t *laps;                /* 00524ea1 */
    int16_t *difficulty;          /* 00524ea3 */
    const int16_t *music_volume;  /* 00524e67 */
    const int16_t *lost;          /* the short 0052b312: the connection lost (no Load / save) */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;             /* the short 00524792 */
    int32_t *screen;              /* 00524834: the screen's picture */
    int32_t *inverse;             /* 004c5ff4: 00466c00's table */
    const int16_t *net_off;       /* the short 0052b302: 004104d0 not called while set */
    const int16_t *quit;          /* the short 0050a5b6: 1 leaves */
    int16_t *redraw_asked;        /* the short 0052b31a: set on leaving */
} OptionsMenuState;

/* 00444bb0 (all registers kept): option/options.qfs's ten items over the
 * pointer's bands: transmission, ABS and traction control (toggled in place
 * unless not offered, 2), audio (004015dc), graphics (00423d28), controls
 * (00413cb0), load / save (0042c498; not while the connection is lost, Up /
 * Down step over it), credits (004166e0 to credits.asf), Done and Cancel
 * (Left / Right between them). Cancel and Escape put the three settings back
 * as they were on entry; after a load / save, Done or leaving otherwise
 * (the 0x2ee0 ticks, the right button, 0050a5b6) set the race's opponent,
 * laps and a tournament's car for the race type again. Returns 0. */
int32_t options_menu(OptionsMenuState *st, const OptionsMenuOps *ops);

#endif
