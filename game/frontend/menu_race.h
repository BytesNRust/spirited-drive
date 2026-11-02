/* The race from the menus (00427ae0): the race run segment by segment, the times kept, the screens after it and
 * the tournament's end. */
#ifndef TNFS_MENU_RACE_H
#define TNFS_MENU_RACE_H

#include <stdint.h>

/* The calls 00427ae0 makes. */
typedef struct MenuRaceOps {
    void (*screen_mode)(void *ctx, int32_t mode);              /* 00475c14 (cdecl) */
    void (*pointer_hide)(void *ctx);                           /* [004c618c] */
    void (*wait_frame)(void *ctx);                             /* 00464a10 */
    void (*clip)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1);  /* 0047723c (cdecl) */
    void (*font_race)(void *ctx);                              /* 00487e10(004c6fac) */
    void (*race)(void *ctx, int32_t argc, char *const *argv);  /* 004275f8: eax argc, edx argv */
    void (*font_set)(void *ctx, int32_t font);                 /* 00487e10 (cdecl): [00524824] */
    void (*present)(void *ctx);                                /* 004735bc */
    void (*palette_write)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*origin)(void *ctx, int32_t x, int32_t y);           /* 004772bc (cdecl) */
    void (*screen_size)(void *ctx, int32_t w, int32_t h, int32_t bits);  /* 00477980 (cdecl) */
    void (*pointer_reset)(void *ctx, int32_t mode);            /* 00441530: eax */
    void (*pointer_draw)(void *ctx);                           /* [004c6190] */
    void (*music_pick)(void *ctx);                             /* 004562e8 */
    void (*cop_stop)(void *ctx);                               /* 00428610: after a race ended 4, the ticket or the cops' movie */
    void (*won)(void *ctx);                                    /* 004283c4: the win movie / winner.qfs, the track marked raced */
    void (*bank_load)(void *ctx);                              /* 004648dc */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    void (*circuit_results)(void *ctx, int32_t finished);      /* 0045eff4: eax */
    void (*segment_results)(void *ctx, int32_t mode);          /* 0045cea0: eax */
    void (*standings)(void *ctx, int32_t segment);             /* 00461540: eax */
    void (*track_results)(void *ctx, int32_t finished);        /* 0045de08: eax */
    void (*tournament_points)(void *ctx);                      /* 0045aec8 */
    void (*after_race)(void *ctx);                             /* 0040c710 */
    int32_t (*next_segment)(void *ctx);                        /* 0040b648: ax 0 to stop */
    void (*tournament_reset)(void *ctx);                       /* 00466a3c */
    int32_t (*champion)(void *ctx, int32_t mode);              /* 0045ae80: eax 0 */
    void (*unlocks)(void *ctx);                                /* 00432e48 */
    void (*game_reset)(void *ctx);                             /* 00466ae4 */
    void *ctx;
    /* port, after ctx so the original's initializers leave it NULL: a race that was run to its end (the FINISHED branch), not one given up or lost */
    void (*finished)(void *ctx);
} MenuRaceOps;

enum { MENU_RACE_IMAGE = 0x5f52, MENU_RACE_GAME = 0x5bc, MENU_RACE_GHOST = 100, MENU_RACE_TIMES = 7 };

/* The globals it reads and writes, through pointers as the original reads memory. */
typedef struct MenuRaceState {
    uint8_t *image;          /* 00524df0: config.dat's image, MENU_RACE_IMAGE bytes */
    uint8_t *game;           /* 0052ad44: the game state, MENU_RACE_GAME bytes */
    int16_t *net_off;        /* the short 0052b302: set while racing */
    int32_t *times;          /* 004fa0cc: a top speed per segment run (from 1), then from 004fa0d8 a total */
    int32_t *ghost;          /* 004f9ea0: the record run the race is against, MENU_RACE_GHOST dwords */
    const int16_t *link;     /* the short 0052b312: a link game */
    const int16_t *link_lost;  /* the short 0052b314 */
    const int16_t *pointer_active;  /* the short 00524790 */
    const int32_t *player;   /* 004fbb08 */
    const int32_t *font;     /* 00524824: the menus' font */
    int32_t *screen;         /* 00524834 */
    int32_t *warrior;        /* 004c5cc4 */
    int32_t *unlock_m;       /* 004baf0c */
    int32_t *unlock_r;       /* 004baf10 */
    int32_t *lost_vegas;     /* 004baf18 */
    const int32_t *host;     /* 004fbb0c */
    int16_t *link_done;      /* the short 0050a5ba */
} MenuRaceState;

/* 00427ae0: while not done: config.dat's 0xac bytes from 00524df9 and the
 * controls kept in the game state (+0x4a8, +0x554); the race run (004275f8
 * with "nfs" and the track: "cy1".. a stage's segment, "tr1".. a circuit)
 * against the record run for the track (and, on a circuit, the
 * difficulty); the counter of segments run (+0x532) up, its top speed and
 * total kept (004fa0cc / 004fa0d8), the track total and best top speed in
 * the player's record; back to config.dat; a finished track (a circuit, or
 * a stage's third segment) beating the best keeps its run; then the
 * screens for the race type, the track and a link game, and, on a stage
 * not yet over, 0040b648 for the next segment. A tournament's end
 * (00466a3c) counts the tracks won: all nine or eight unlock the rest. */
void menu_race(const MenuRaceState *st, const MenuRaceOps *ops);

#endif
