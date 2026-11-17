/* port (dep59 step 4, Task 4): the records an edition shows (notes/dep59-step4-editions-plan.md, section 4). The standard edition reads
 * race_records.dat as before; an edition of its own joins the tables of its cars that are there (race_records_added.h: every Restomod
 * race writes its car's table) and shows the ten best of them. A car's table holds its ten best, so the ten best of the joined tables
 * are the ten best those cars ever set. Equal times keep the edition's car order (the rows hold no date, so which was set first is not
 * known; race_records.dat keeps the first one set first). */
#ifndef TNFS_RACE_RECORDS_MERGE_H
#define TNFS_RACE_RECORDS_MERGE_H

#include "formats/edition_ini.h"
#include "game/sys/race_records.h"

/* The table of the key (race_records_find's) joined over the cars `ids` (`n` of them, stock or added ids): the ten best totals, best
 * first; the top speed; on a stage each segment's best time and top speed; the best run's figures with the first row's. An added car's
 * rows take its number for the screens (car_registry_find; one with no piece loaded keeps none). 1 when a car has a table there; 0, and
 * `out` empty, when none has */
int race_records_cars_merge(int track, int layout, int cheats, int laps, const char *const ids[], int n, RecordTable *out);
/* the best lap of (track, layout, cheats) over the cars `ids`, as above: 1; 0 and `out` empty when none has one */
int race_records_cars_lap_merge(int track, int layout, int cheats, const char *const ids[], int n, RecordRow *out);
/* The same over race_records_tracks.dat's tables of the track `track` (an id: "lunar", a mod's), one per car (dep59 L6) */
int race_records_tracks_merge(const char *track, int layout, int cheats, int laps, const char *const ids[], int n, RecordTable *out);
int race_records_tracks_lap_merge(const char *track, int layout, int cheats, const char *const ids[], int n, RecordRow *out);
/* the place of the row `row` among `rows` (`n` of them): the first the same but for its car words (an added car's are 0 in its table and
 * its number in a joined one), -1 when it is not there */
int race_records_row_index(const RecordRow rows[], int n, const RecordRow *row);

/* the cars of the edition in use that are there (edition_car_present) into `ids`, pointing into `e`: how many; 0 in the standard edition,
 * outside Restomod and for an edition that cannot be used (which then is the standard one) */
int race_records_edition_cars(Edition *e, const char *ids[], int cap);

/* What the screens read for a key: in an edition of its own its cars' joined table (a copy, kept until the next call), NULL when none of
 * them has one; else race_records_find's */
const RecordTable *race_records_shown(int track, int layout, int cheats, int laps);
/* the best lap the same way (race_records_lap_find's in the standard edition) */
const RecordRow *race_records_shown_lap(int track, int layout, int cheats);
/* the lap counts with a table the same way, ascending, at most `max` (race_records_laps' in the standard edition); how many */
int race_records_shown_laps(int track, int layout, int cheats, int *out, int max);

/* What Best Times shows for a track of race_records_tracks.dat ("lunar", a mod's): the tables of the edition's cars joined in an edition of its
 * own, else of every car in the file that has one for the track (race_records_tracks_cars). The same three calls as the stock tracks' above;
 * NULL when no car has a table there (a copy of the join, kept until the next call) */
const RecordTable *race_records_shown_track(const char *track, int layout, int cheats, int laps);
const RecordRow *race_records_shown_track_lap(const char *track, int layout, int cheats);
int race_records_shown_track_laps(const char *track, int layout, int cheats, int *out, int max);

#endif
