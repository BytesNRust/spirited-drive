/* The saved games' and the replays' list: load or delete one (0042ae9c). */
#ifndef TNFS_GAME_LIST_H
#define TNFS_GAME_LIST_H

#include "game/frontend/yes_no_box.h"

#include <stdint.h>

enum {
    GAME_LIST_LOAD_REPLAY = 0, GAME_LIST_DELETE_REPLAY = 1, GAME_LIST_LOAD_GAME = 2, GAME_LIST_DELETE_GAME = 3,
    GAME_LIST_NAMES = 100,  /* names of FILE_LIST_NAME bytes at 004fa1dc */
    GAME_LIST_PATH = 0x50,  /* 0042ae9c's paths: the working directory's, the saves' or replays' */
    GAME_LIST_STATE = 0x5b6 /* the game state 0052ad44, kept at 00524838 before a load */
};

typedef struct GameListOps {
    void (*text_styles)(void *ctx);                                        /* 0046376c */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                  /* the picture's dword +0x2c */
    void (*select)(void *ctx, int32_t picture);                            /* 00473680 (cdecl) */
    void (*grab)(void *ctx, int32_t shape, int32_t x, int32_t y);          /* 00476e28 (cdecl): the screen into shape */
    int32_t (*load_art)(void *ctx, const char *name);                      /* 00466c34: eax the name; the bank */
    void (*music_feed)(void *ctx);                                         /* 004640d0 */
    void (*getcwd)(void *ctx, char *out);                                  /* 00488814 (cdecl): 0x1ff at most */
    void (*chdir)(void *ctx, const char *path);                            /* 00488ffa: eax */
    const char *(*find_first)(void *ctx, const char *pattern);             /* 00489084 (cdecl): a name or NULL */
    const char *(*find_next)(void *ctx);                                   /* 00489020: a name or NULL */
    void (*clip)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1);  /* 0047723c (cdecl) */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);              /* 00464038: eax, edx */
    int32_t (*ticks)(void *ctx);                                           /* 00473334 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*sound)(void *ctx, int32_t sound);                               /* 0046492c: eax */
    void (*deadline_set)(void *ctx, int32_t time);                         /* 0047334f (cdecl) */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);     /* 004732dd (cdecl) */
    void (*blit_shape_at)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);  /* 004737d0 (cdecl) */
    void (*text_select)(void *ctx, int32_t style);                         /* 004638c0: eax */
    void (*text_fit)(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);  /* 00463d34 */
    void (*blit_masked_at)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);  /* 00473a98 (cdecl) */
    void (*select_default)(void *ctx);                                     /* 00473604 */
    void (*lock)(void *ctx);                                               /* 00473578 */
    void (*blit_own)(void *ctx, int32_t shape);                            /* 004737ec (cdecl) */
    void (*blit_at)(void *ctx, int32_t shape, int32_t x, int32_t y);       /* 004737d0 (cdecl): a picture's shape */
    void (*unlock)(void *ctx);                                             /* 004733b0 */
    void (*flip)(void *ctx);                                               /* 004733ec */
    void (*pointer_show)(void *ctx);                                       /* 00441798 */
    int32_t (*deadline_passed)(void *ctx);                                 /* 0047336d */
    void (*wait)(void *ctx);                                               /* 00475c14(0) (cdecl) */
    int32_t (*pointer_buttons)(void *ctx);                                 /* 004417f8 */
    void (*pointer_activate_if_moved)(void *ctx);                          /* 00464088 */
    void (*music)(void *ctx);                                              /* 0046417c */
    int32_t (*key)(void *ctx);                                             /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                      /* 00441814 */
    int32_t (*yes_no)(void *ctx, const char *text);                        /* 00465f4c: eax */
    void (*remove_file)(void *ctx, const char *path);                      /* 0048855b: eax */
    void (*banner_wait)(void *ctx, const char *text, int32_t time);        /* 00467214: eax, edx */
    void (*game_load)(void *ctx, const char *name);                        /* 0042a484: eax (game_file_load) */
    void (*music_fade_to)(void *ctx, int32_t volume);                      /* 00464604: eax */
    void (*pointer_redraw)(void *ctx);                                     /* 00441874 */
    void (*pointer_off)(void *ctx);                                        /* 00464060 */
    void (*blit_masked)(void *ctx, const uint8_t *shape);                  /* 00473ab4 (cdecl) */
    void (*picture_free)(void *ctx, int32_t picture);                      /* 004746a0 (cdecl) */
    void (*free)(void *ctx, int32_t bank);                                 /* 00475260 (cdecl) */
    void *ctx;
} GameListOps;

typedef struct GameListState {
    int german;                 /* the short 0052b31c */
    int16_t check;              /* the short 004fa9ac: check/loadanim.qfs, else option/'s */
    int16_t highlight;          /* the short 004bad00: "Load highlight ?" for "Load replay ?" */
    int32_t screen;             /* 00524834 */
    const char *save_dir;       /* 005241e8: paths.dat's slot 1 */
    const char *replay_dir;     /* 00524418: paths.dat's slot 8 */
    uint8_t *names;             /* 004fa1dc: GAME_LIST_NAMES + 1 names of FILE_LIST_NAME bytes */
    int16_t *pointer_active;    /* the short 00524790 */
    int16_t *buttons;           /* the short 00524792 */
    uint8_t *config;            /* 00524df9: config.dat's image (track_select.h's offsets), 0xac bytes */
    char *replay;               /* 00524e2b: the replay's name (cut at its '.'; a name of FILE_LIST_NAME fits) */
    uint8_t *game;              /* 0052ad44: the game state, GAME_LIST_STATE bytes */
    uint8_t *game_kept;         /* 00524838: the game state before a load */
    uint8_t *controls;          /* 0052ad26: the controls, 0x1a bytes */
    int32_t *lost_vegas;        /* 004baf18 */
    int16_t *tournament;        /* the short 0052b310 */
} GameListState;

/* 0042ae9c: the names matching *.SAV in the saves' directory (modes 2, 3) or
 * *.RPL in the replays' (0, 1; the system replays dropped in 1), ten at a
 * time in a window that opens and closes in seven frames; the pointer or
 * the arrows pick a name, the scroll arrows or Cancel; Enter, space or a
 * button act: mode 0 asks to load the replay (its name to 00524e2b, 1), 1
 * and 3 to delete the file (the list read again; not a system replay), 2 to
 * load the game (the state kept at 00524838, the players' records cleared,
 * the config, controls and Lost Vegas from it, a tournament, the music's
 * volume; 1 when the track is below 3 and 00524e83 above 0). Escape, the
 * right button or Cancel leave. The working directory is put back. */
int32_t game_list_screen(int32_t mode, GameListState *st, const GameListOps *ops);

#endif
