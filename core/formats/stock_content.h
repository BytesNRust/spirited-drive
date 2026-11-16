/* port (dep59 step 4): the ids an edition (edition_ini.h) names the game's own cars and tracks by, fixed for good: a file written today
 * must mean the same pieces in every later version (notes/dep59-step4-editions-plan.md, sections 1 and 2).
 *   cars, by their number in the menus: 0 rx7, 1 nsx, 2 supra, 3 911, 4 zr1, 5 viper, 6 512tr, 7 diablo, 11 warrior (8 PACK, 9 CLOCK and
 *        10 CLASS are the records' columns, no car)
 *   tracks, by theirs: 0 city, 1 coastal, 2 alpine, 3 rusty, 4 autumn, 5 burnt, 6 vertigo, 7 transtropolis, 8 vegas, 9 hidden
 *        (rusty covers Lunar Springs: a switch on its rally, not a track of its own; hidden, the cut moon track, has no geometry on the
 *        CD and is never listed: its id is kept so that it never means anything else) */
#ifndef TNFS_STOCK_CONTENT_H
#define TNFS_STOCK_CONTENT_H

enum { STOCK_CAR_WARRIOR = 11, STOCK_TRACK_COUNT = 10 };

/* the stock car `car`'s id; NULL when `car` is none of them */
const char *stock_car_id(int car);

/* the number of the stock car whose id is `id`; -1 when none */
int stock_car_of_id(const char *id);

/* the stock track `track`'s id; NULL when `track` is none of them */
const char *stock_track_id(int track);

/* the number of the stock track whose id is `id`; -1 when none */
int stock_track_of_id(const char *id);

#endif
