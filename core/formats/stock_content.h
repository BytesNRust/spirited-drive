/* port (dep59 step 4): the ids an edition (edition_ini.h) names the game's own cars and tracks by, fixed for good: a file written today
 * must mean the same pieces in every later version (notes/dep59-step4-editions-plan.md, sections 1 and 2).
 *   cars, by their number in the menus: 0 rx7, 1 nsx, 2 supra, 3 911, 4 zr1, 5 viper, 6 512tr, 7 diablo, 11 warrior (8 PACK, 9 CLOCK and
 *        10 CLASS are the records' columns, no car)
 *   tracks, by theirs: 0 city, 1 coastal, 2 alpine, 3 rusty, 4 autumn, 5 burnt, 6 vertigo, 7 transtropolis, 8 vegas, 9 hidden,
 *        10 lunar (hidden, the cut moon track, has no geometry on the CD and is never listed: its id is kept so that it never means
 *        anything else; lunar, Lunar Springs, is Rusty's rally on the moon's art, an entry of its own since dep59 Lunar L1:
 *        notes/dep59-lunar-entry-l1l2l5-addendum.md. The menus' track number never holds 10: it is (3, rally, the Lunar pick)) */
#ifndef TNFS_STOCK_CONTENT_H
#define TNFS_STOCK_CONTENT_H

enum { STOCK_CAR_WARRIOR = 11, STOCK_TRACK_LUNAR = 10, STOCK_TRACK_COUNT = 11 };

/* the stock car `car`'s id; NULL when `car` is none of them */
const char *stock_car_id(int car);

/* the number of the stock car whose id is `id`; -1 when none */
int stock_car_of_id(const char *id);

/* the stock track `track`'s id; NULL when `track` is none of them */
const char *stock_track_id(int track);

/* the number of the stock track whose id is `id`; -1 when none */
int stock_track_of_id(const char *id);

/* port (dep59 step 4b): the English names the CONTENT screen shows the stock pieces by; NULL when `car` or `track` is none of them, and for
 * the hidden track, which is never listed */
const char *stock_car_name(int car);
const char *stock_track_name(int track);

#endif
