/* The second load / save menu (0042d894): 0042c498's twin, compiled apart, its items offered by the
 * tournament's progress. */
#ifndef TNFS_LOAD_SAVE2_H
#define TNFS_LOAD_SAVE2_H

#include "game/frontend/load_save.h"

typedef struct LoadSave2State {
    int german;               /* the short 0052b31c */
    int16_t tournament;       /* the short 0052b310 */
    int16_t lost;             /* the short 0052b312: the connection lost */
    int16_t race_type;        /* config.dat's short 00524e5d: 3 a tournament */
    int16_t pointer_active;   /* the short 00524790 */
    int16_t *buttons;         /* the short 00524792 */
    const int16_t *races;     /* the shorts 0052b2d6..0052b2e4: the tournament's races, summed (8 its end) */
    int16_t extra;            /* the short 0052b2e6: Lost Vegas's, counted (out of nine) when it is open */
    int32_t ninth;            /* 004baf18: Lost Vegas open */
    int32_t *screen;          /* 00524834: the screen's picture */
    int32_t *inverse;         /* 004c5ff4: 00466c00's table */
    int16_t *check;           /* the short 004fa9ac: 1 with check/'s art (set on entry only) */
    int16_t *highlight;       /* the short 004bad00: 0042ae9c's "Load highlight ?" */
    int16_t *replay_flag;     /* config.dat's short 00524e9f: 1 a replay, 2 a highlight while it plays */
    int16_t *music_silent;    /* the short 004c5cbe: when set, cleared and the volumes put back first */
    int16_t *volumes;         /* config.dat's shorts 00524e65, 00524e67, 00524e69 */
    int16_t kept[3];          /* the shorts 0052b322, 0052b326, 0052b328: put back into them */
    int16_t mode_switch;      /* config.dat's short 00524e6f: the video mode set again after the replay */
    const char *name;         /* 00524df9: the player's name, config.dat's first bytes */
    const char *replay_name;  /* 00524e2b */
} LoadSave2State;

/* 0042d894 (all registers kept): 0042c498's menu (the art by eax, as a short, 1 or not; 004fa9ac set) over
 * the same bands but for Load's, which it has none of. Saving is offered in a tournament (config.dat's race
 * type 3) not run to its end, viewing unless the connection was lost; Up / Down step over Load and the
 * others. The choices are 0042c498's, but a load leaves the menu open (the pointer, when off, on Delete
 * game) and nothing is kept around a replay. Escape, Done, the right button or the 0x2ee0 ticks run out
 * leave. Returns 0. */
int32_t load_save_menu2(int32_t mode, LoadSave2State *st, const LoadSaveOps *ops);

#endif
