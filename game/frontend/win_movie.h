/* After a race the player won: the win movie or winner.qfs (004283c4). */
#ifndef TNFS_WIN_MOVIE_H
#define TNFS_WIN_MOVIE_H

#include "game/frontend/cop_stop.h"
#include "game/frontend/tournament.h"

#include <stdint.h>

typedef struct WinMovieState {
    uint8_t *game_state;          /* 0052ad44: the totals (+0x65), type 3's wins (+0x580) and races (+0x592) by
                                   * track, 0052b2fc (+0x5b8) the movie rather than winner.qfs */
    int32_t player;               /* 004fbb08 */
    int32_t racers;               /* 004fb7dc */
    int16_t view;                 /* 0052b316: only watching */
    const int16_t *dropped;       /* 004f82a0: a short per player, set when it did not finish */
    int16_t race_type;            /* 00524e5d */
    int16_t track;                /* 00524e85 */
    int16_t level;                /* 00524e8b */
    int32_t lost_vegas;           /* 004baf18 */
    int32_t *screen;              /* 00524834: the screen's picture, a new 640 x 480 one for winner.qfs */
} WinMovieState;

/* 004283c4 (all registers kept; 00428610's ops): the track's race is marked run (the short at +0x592 + 2 x track);
 * the player won when its total (+0x65) is above 0, it is not only watching, did not drop out and no racer's total
 * is below it (ties won). Then: race type 3 marks the track won (+0x580 + 2 x track) and, the nine tracks' wins
 * summed, all nine at 9 with Lost Vegas or the first eight at 8 without, win#7.tgv (winner.qfs on a new screen picture, 00466f68 mode 0,
 * while 0052b2fc is 0); else the track's movie (win#2, 4, 6, 1, 3, 3, 5, 5; win#5 past track 7). Other race types
 * but 0: win#N.tgv, N 00466f34's pick (asked again while 2) plus 1, 3 or 5 for levels below 3, below 6 or above. */
void win_movie(const WinMovieState *st, const CopStopOps *ops);

#endif
