/* The menu after a race (check/finmenu.qfs): continue, the replay, the highlights, the options, the results,
 * the chat (0040c710). */
#ifndef TNFS_AFTER_RACE_H
#define TNFS_AFTER_RACE_H

#include <stdint.h>

enum { AFTER_RACE_PALETTE = 0x300, AFTER_RACE_SAVED = 0x5b6, AFTER_RACE_KEPT_AT = 0x5ba, AFTER_RACE_CARS = 8,
       AFTER_RACE_CAR = 0x1d2 };

typedef struct AfterRaceOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);                    /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    void (*palette_clear)(void *ctx, uint8_t *rgb);                          /* 00463df0: eax */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194] */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    void (*net_service)(void *ctx);                                          /* 004104d0 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    int32_t (*chat_waiting)(void *ctx);                                      /* 004109b4 (ax) */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*blit_masked)(void *ctx, const uint8_t *shape);                    /* 00473ab4 (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    void (*music_fade_out)(void *ctx);                                       /* 004644f0 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*music_end)(void *ctx);                                            /* 00464568 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*picture_free)(void *ctx, int32_t picture);                        /* 004746a0 (cdecl) */
    void (*sound_free)(void *ctx);                                           /* 004649c8 */
    void (*clear_one)(void *ctx);                                            /* 00464a10 (screen_clear_one) */
    void (*window)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h);   /* 0047723c (cdecl) */
    void (*replay)(void *ctx, int32_t mode, const char *path);               /* 004275f8: eax 2, edx */
    void (*video_reset)(void *ctx);                                          /* 004735bc */
    void (*palette_put)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*mode_set)(void *ctx, int32_t w, int32_t h, int32_t bits);         /* 00477980 (cdecl) */
    void (*pointer_reset)(void *ctx, int32_t mode);                          /* 00441530: eax */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    void (*sound_load)(void *ctx);                                           /* 004648dc */
    void (*music_pick)(void *ctx);                                           /* 004562e8 */
    void (*options)(void *ctx, int32_t mode);                                /* 00403760: eax 1 */
    void (*standings)(void *ctx, int32_t mode);                              /* 00461540: eax */
    void (*circuit_results)(void *ctx, int32_t mode);                        /* 0045eff4: eax 0 */
    void (*segment_results)(void *ctx, int32_t mode);                        /* 0045cea0: eax 1 */
    void (*track_results)(void *ctx, int32_t mode);                          /* 0045de08: eax 0 */
    void (*chat)(void *ctx);                                                 /* 0040a5d8 */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void *ctx;
} AfterRaceOps;

typedef struct AfterRaceState {
    const int16_t *link;            /* the short 0052b312: a link game (its connection lost: the offline items) */
    const int16_t *link_over;       /* the short 0050a5b6: 1 when the link game's tournament is over */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;               /* the short 00524792 */
    int32_t *screen;                /* 00524834: the screen's picture */
    int32_t *inverse;               /* 004c5ff4: 00466c00's table */
    int32_t *clock;                 /* 004bb8dc: the ticks counted for the chat's blink */
    int16_t *chat_closed;           /* the short 0052b31a: set when the chat ended */
    int16_t *replay_flag;           /* config.dat's short 00524e9f: 1 the replay, 2 the highlights while it plays */
    char *replay_name;              /* config.dat's 00524e2b: REPLAY.RPL written into it */
    int16_t *music_silent;          /* the short 004c5cbe: when set, cleared and the volumes put back first */
    int16_t *volumes;               /* config.dat's shorts 00524e65, 00524e67, 00524e69 */
    int16_t kept[3];                /* the shorts 0052b322, 0052b326, 0052b328: put back into them */
    int16_t mode_switch;            /* config.dat's short 00524e6f: the video mode set again after the replay */
    int16_t track;                  /* config.dat's short 00524e85: above 2 a circuit */
    int16_t race_type;              /* config.dat's short 00524e5d: 2, 3 (a tournament) or another */
    uint8_t *game_state;            /* 0052ad44: AFTER_RACE_SAVED bytes kept around the replay; the short at
                                     * AFTER_RACE_KEPT_AT (0052b2fe) kept at 1 when it was 1 */
    uint8_t *cars;                  /* 0051ce24: a short each AFTER_RACE_CAR bytes, the cars' points, kept too */
} AfterRaceState;

/* 0040c710 (all registers kept): eax the result, 1 (Continue, Escape, the deadline, the right button or a link
 * game's tournament over). Six items over the pointer's bands (Continue; the replay and the highlights not in a
 * link game; the options; the results; the chat only in one). Enter, Space or the left button choose once the keys
 * are taken: the replay (REPLAY.RPL, 00524e9f 1 or 2) with the menu freed and the game state and cars' points kept
 * around it, the options (00403760), the results screens by the track and race type, the chat (0040a5d8). The chat
 * item blinks (every 0x1e clock ticks) while a message waits. */
int32_t after_race_menu(AfterRaceState *st, const AfterRaceOps *ops);

#endif
