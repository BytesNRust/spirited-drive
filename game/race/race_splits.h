/* The split times (0051c808..0051cc50): five checkpoints a lap on the
 * player's progress, the tick each was passed, the same for the rival the
 * player is measured against, and the gap the dashboard shows. */
#ifndef TNFS_RACE_SPLITS_H
#define TNFS_RACE_SPLITS_H

#include <stdint.h>

enum {
    RACE_SPLITS = 85                /* 17 laps of 5 (0045aab4) */
};

#pragma pack(push, 1)
typedef struct RaceSplits {
    int32_t checkpoints[RACE_SPLITS];   /* 0051c808: progress past which each split counts (0045aab4) */
    int32_t unknown_154[5];
    int32_t passed;                     /* 0051c970: the player's splits passed */
    int32_t rival_passed;               /* 0051c974: the rival's */
    int32_t passed_ticks[RACE_SPLITS];  /* 0051c978: the tick each was passed */
    int32_t unknown_2c4[5];
    int32_t rival_ticks[RACE_SPLITS];   /* 0051cae0 */
    int32_t unknown_42c[5];
    int32_t gap;                        /* 0051cc48: rival minus player at the last split both passed (0045b1d0) */
    int32_t gap_shown;                  /* 0051cc4c: ticks left showing a new gap */
} RaceSplits;
#pragma pack(pop)
typedef char race_splits_size_check[sizeof(RaceSplits) == 0x448 ? 1 : -1];

#endif
