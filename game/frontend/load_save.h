/* The load / save menu: games, replays and highlights (0042c498). */
#ifndef TNFS_LOAD_SAVE_H
#define TNFS_LOAD_SAVE_H

#include <stdint.h>

enum {
    LOAD_SAVE_PALETTE = 0x300, /* 0042c498's two palettes on its stack */
    LOAD_SAVE_PATH = 0x50,     /* its path buffer */
    LOAD_SAVE_RACE = 0x2c      /* 004fbad8..004fbb03: kept around the replay */
};

typedef struct LoadSaveOps {
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
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    int32_t (*game_list)(void *ctx, int32_t mode);                           /* 0042ae9c: eax (game_list.h) */
    void (*save_box)(void *ctx, char *path);                                 /* 0042a680: eax (save_box_show) */
    void (*replay_box)(void *ctx, char *path);                               /* 0042a9f8: eax (replay_box_show) */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*music_fade_out)(void *ctx);                                       /* 004644f0 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*music_end)(void *ctx);                                            /* 00464568 */
    void (*picture_free)(void *ctx, int32_t picture);                        /* 004746a0 (cdecl) */
    void (*sound_free)(void *ctx);                                           /* 004649c8 */
    void (*clear_one)(void *ctx);                                            /* 00464a10 (screen_clear_one) */
    void (*window)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h);   /* 0047723c (cdecl) */
    void (*replay)(void *ctx, int32_t mode, const char *path);               /* 004275f8: eax 2, edx */
    void (*video_reset)(void *ctx);                                          /* 004735bc */
    void (*palette_put)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*mode_set)(void *ctx, int32_t w, int32_t h, int32_t bits);         /* 00477980 (cdecl) */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    void (*sound_load)(void *ctx);                                           /* 004648dc */
    void (*music_pick)(void *ctx);                                           /* 004562e8 */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void *ctx;
} LoadSaveOps;

typedef struct LoadSaveState {
    int german;               /* the short 0052b31c */
    int16_t tournament;       /* the short 0052b310 */
    int16_t lost;             /* the short 0052b312: the connection lost */
    int16_t race_type;        /* config.dat's short 00524e5d: 3 a tournament */
    int16_t pointer_active;   /* the short 00524790 */
    int16_t *buttons;         /* the short 00524792 */
    int32_t *screen;          /* 00524834: the screen's picture */
    int32_t *inverse;         /* 004c5ff4: 00466c00's table */
    int16_t *check;           /* the short 004fa9ac: 1 with check/'s art */
    int16_t *variant;         /* config.dat's short 00524e83: cleared after a load */
    int16_t *highlight;       /* the short 004bad00: 0042ae9c's "Load highlight ?" */
    int16_t *replay_flag;     /* config.dat's short 00524e9f: 1 a replay, 2 a highlight while it plays */
    uint8_t *race;            /* 004fbad8: LOAD_SAVE_RACE bytes kept around the replay */
    int16_t *music_silent;    /* the short 004c5cbe: when set, cleared and the volumes put back first */
    int16_t *volumes;         /* config.dat's shorts 00524e65, 00524e67, 00524e69 */
    int16_t kept[3];          /* the shorts 0052b322, 0052b326, 0052b328: put back into them */
    int16_t mode_switch;      /* config.dat's short 00524e6f: the video mode set again after the replay */
    const char *name;         /* 00524df9: the player's name, config.dat's first bytes */
    const char *replay_name;  /* 00524e2b */
} LoadSaveState;

/* 0042c498 (all registers kept): the menu of check/loadsave.qfs (eax, as a
 * short, 1; 004fa9ac set) or option/'s over eight bands of the pointer
 * (taller ones in German): save, load, delete a game, save the replay, view
 * the replay or the highlight, delete a replay, done. Saving is offered
 * only from 0 in a tournament, loading outside one, viewing unless the
 * connection was lost; Up / Down step over the others. Saving asks 0042a680
 * for config.dat's name, the replay 0042a9f8; loading, viewing and deleting
 * are 0042ae9c's modes 2, 0 and 3 / 1; a replay chosen there plays
 * (004275f8) between the menu's art freed and loaded again. Escape, Done,
 * the right button or the 0x2ee0 ticks run out leave. Returns 0. */
int32_t load_save_menu(int32_t mode, LoadSaveState *st, const LoadSaveOps *ops);

#endif
