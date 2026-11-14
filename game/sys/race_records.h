/* port (dep55, Restomod's records): one store for every race's records, "race_records.dat", behind port_records_unified (Stock and Restored
 * keep the original's config.dat records, mixed, as the exe does). A table is keyed by the track, the layout (any of mirror, reverse and
 * rally: eight), the cheats (on or off; each row says which) and the lap count (1..99; 0 for a stage, which has none). It holds the ten
 * best totals, the top speed and, on a stage, each segment's best time and top speed. The best lap is keyed by the track, the layout and
 * the cheats alone: a lap is a lap whatever the race's length. Every row is the exe's 0x27-byte record (record_check.h's routines run on
 * it unchanged) with the marks of the race that set it: its cheats, weather, time of day and difficulty. A table starts empty; an empty
 * row has no name, the longest time and no speed, so any finished race beats it. */
#ifndef TNFS_RACE_RECORDS_H
#define TNFS_RACE_RECORDS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    RACE_RECORDS_TRACKS = 9, RACE_RECORDS_FIRST_CIRCUIT = 3, RACE_RECORDS_LAPS_MAX = 99,
    RACE_RECORDS_ROW = 0x27, RACE_RECORDS_ROWS = 10, RACE_RECORDS_SEGMENTS = 3,
    /* a row's fields (record_check.c's RecordAt, from the row's start) */
    RECORD_ROW_CAR = 0x0f, RECORD_ROW_CAR_B = 0x13, RECORD_ROW_SET = 0x17, RECORD_ROW_TIME = 0x1b, RECORD_ROW_SPEED = 0x1f,
    RECORD_ROW_GEARBOX = 0x23, RECORD_ROW_NAME_MAX = RECORD_ROW_CAR
};

/* the layout: a bit each, all eight combinations a table of their own */
enum { LAYOUT_NORMAL = 0, LAYOUT_MIRROR = 1, LAYOUT_REVERSE = 2, LAYOUT_RALLY = 4, RACE_RECORDS_LAYOUTS = 8 };

/* A row's marks. `cheats` is EXTRAS_MARK_* (extras_marks.h). The other three are 0 when not known (a row from before them: an imported
 * one), else RECORD_MARK_KNOWN with: `weather` a WEATHER_* (weather.h) in the low three bits, RECORD_WEATHER_SNOW for a race on snow and
 * RECORD_WEATHER_WET for the wet grip (rain_grip); `time` a LIGHT_TIME_* (port_settings.h) (a race on the original's own time of day
 * takes its nearest: morning DAWN, midday DAY, evening DUSK); `difficulty` the rivals', the police's and the traffic's levels (difficulty.h, 0..2), two bits each. */
enum {
    RECORD_MARK_KNOWN = 0x80, RECORD_WEATHER_KIND = 0x07, RECORD_WEATHER_SNOW = 0x08, RECORD_WEATHER_WET = 0x10, RECORD_TIME_KIND = 0x0f
};
typedef struct {
    uint8_t cheats, weather, time, difficulty;
} RecordMarks;

typedef struct {
    uint8_t row[RACE_RECORDS_ROW];
    RecordMarks marks;
} RecordRow;

typedef struct {
    RecordRow totals[RACE_RECORDS_ROWS];   /* the ten best totals, best first */
    RecordRow top;                          /* the top speed (a circuit's) */
    RecordRow segment_time[RACE_RECORDS_SEGMENTS], segment_speed[RACE_RECORDS_SEGMENTS];   /* a stage's segments */
    int32_t stage_speeds[RACE_RECORDS_SEGMENTS], stage_totals[RACE_RECORDS_SEGMENTS];       /* a stage's best run's (record_check_totals) */
} RecordTable;

/* The conditions of the race now running, as a row's marks (`cheats` 0): set at its start (track_viewer.c), read when its records are
 * taken. `light_time` a LIGHT_TIME_*; each difficulty axis 0..2. */
/* The LIGHT_TIME_* of the original's time of day (0 morning, 1 midday, 2 evening): DAWN, DAY, DUSK */
int race_records_original_time(int time_of_day);
RecordMarks race_records_conditions_of(int weather, int snow, int wet, int light_time, int rivals, int police, int traffic);
void race_records_conditions_set(RecordMarks m);
RecordMarks race_records_conditions(void);
/* The race now running had the moon's gravity in effect (set at its start): its records take the cheat EXTRAS_MARK_LUNAR */
void race_records_lunar_set(int on);
int race_records_lunar(void);

/* An empty row, and whether a row holds a record */
void race_records_row_empty(RecordRow *r);
int race_records_row_set(const RecordRow *r);

/* The table of a key, NULL when there is none (or the key is out of range: a stage takes laps 0, a circuit 1..99). The pointer stays
 * valid until race_records_reset or a load. */
RecordTable *race_records_find(int track, int layout, int cheats, int laps);
/* The same, made empty when absent (nothing is saved); NULL out of range or without memory */
RecordTable *race_records_make(int track, int layout, int cheats, int laps);
/* The lap counts (track, layout, cheats) has a table for, ascending, at most `max`; how many */
int race_records_laps(int track, int layout, int cheats, int *out, int max);

/* The best lap of (track, layout, cheats), shared by every lap count; find and make as the tables' */
RecordRow *race_records_lap_find(int track, int layout, int cheats);
RecordRow *race_records_lap_make(int track, int layout, int cheats);

/* The cheats a tournament in progress began with (EXTRAS_MARK_*), kept so a tournament taken up later ends in its own tables */
int race_records_tournament(void);
void race_records_set_tournament(int marks);

typedef enum { RACE_RECORDS_LOADED, RACE_RECORDS_NO_FILE, RACE_RECORDS_DAMAGED } RaceRecordsLoad;
/* "race_records.dat" into the store (emptied first). A damaged file is logged and copied to "race_records.bad" ("race_records.bad2" ..
 * when one is there already) before anything can save over it; the store is then empty, and when no copy could be kept saving stays off
 * (race_records_hold) so the file is not lost. */
RaceRecordsLoad race_records_load(void);
/* the store to the file; false, and nothing written, while saving is held */
bool race_records_save(void);
/* saving off (1) for the session, or on again (0): a file that is there but could not be read, or a damaged one with no copy kept */
void race_records_hold(int on);
/* drops every table and lap (the tests', and a load's start) */
void race_records_reset(void);

#endif
