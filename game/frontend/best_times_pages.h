/* port (dep55): the Best Times screen's pages with Restomod's records (best_times.h's BestTimesPlus over race_records.h): the title strip,
 * the pages' rows, the fields' steps. best_times.c keeps the exe's screen round them (its art, its footer, its keys' loop). */
#ifndef TNFS_BEST_TIMES_PAGES_H
#define TNFS_BEST_TIMES_PAGES_H

#include "game/frontend/best_times.h"
#include "game/sys/race_records.h"

/* the strip's items the pointer can be on (best_times.c's items past its own), and their hit rects as the last draw placed them */
enum {
    BEST_PAGES_LAYOUT_DOWN = 0, BEST_PAGES_LAYOUT_UP, BEST_PAGES_CHEATS, BEST_PAGES_LAPS_DOWN, BEST_PAGES_LAPS_UP, BEST_PAGES_PAGE_DOWN,
    BEST_PAGES_PAGE_UP, BEST_PAGES_CHEATS_DOWN, BEST_PAGES_CHEATS_UP, BEST_PAGES_ITEMS
};
typedef struct {
    int16_t x0[BEST_PAGES_ITEMS], x1[BEST_PAGES_ITEMS], y0[BEST_PAGES_ITEMS], y1[BEST_PAGES_ITEMS];
} BestPagesHits;

/* The pages a track has: 2, the ten best totals then a circuit's best lap or a stage's segments (the top speed is under every page) */
int best_times_pages_count(int track);

/* `plus` opened on a table: the track screen's layout, cheats and laps (0 on a stage; on a circuit 0 takes the menu's choice's count) */
void best_times_pages_open(BestTimesPlus *plus, int track, int layout, int cheats, int laps, int laps_choice);
/* another track on the screen: its laps as the menu's (or laps=N's, `free_laps`), the page back to the first when it has fewer */
void best_times_pages_track(BestTimesPlus *plus, int track, int free_laps, int laps_choice);

/* An item's step (BEST_PAGES_*), every field a carousel (past the last value the first, before the first the last): the layout round all
 * eight, the cheats over (its word and both its arrows), the laps through the track's own and those with a table, the page */
void best_times_pages_act(BestTimesPlus *plus, int track, int item);
/* the key's row's step: `dir` -1 or 1 (Enter: 1); nothing on the footer's row */
void best_times_pages_step(BestTimesPlus *plus, int track, int dir);
/* Up (-1) or Down (1): the keys' row on, over the laps on a stage */
void best_times_pages_focus(BestTimesPlus *plus, int track, int dir);

/* The strip and the page, over the art (the exe's own rows are not drawn); `lit` the pointer's item (BEST_PAGES_*, -1 none); `hits` the
 * items' rects */
void best_times_pages_draw(const BestTimesInputs *in, const BestTimesState *st, int lit, BestPagesHits *hits, const BestTimesOps *ops);

/* The words of a row's conditions, four letters each: weather (CLR FOG RAIN STRM, SNOW, WET for the wet grip), time of day (DAWN DAY DUSK
 * NGT), difficulty (O Original, E Easy, H Hard, M when the axes differ): "RAIN DUSK H"; "-" for a row from
 * before them */
void best_times_conditions_text(RecordMarks m, char *out, int size);

#endif
