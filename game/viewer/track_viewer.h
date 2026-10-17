/* Development track viewer (see track_viewer.c). */
#ifndef TNFS_TRACK_VIEWER_H
#define TNFS_TRACK_VIEWER_H

#include <stdint.h>

/* name: track file stem such as "TR1" or "CY1". */
int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot);

enum { TRACK_VIEWER_RACERS = 8 };  /* the most cars a drive races (RACE_PLAYERS) */

/* Drive car `model` (car_model_names) on the track against racers - 1
 * computer cars (the other models); race_type 0..3 instead sets the cars
 * as 0042702c (type 1 on CY, CL, AL: one rival, the police and traffic),
 * -1 keeps `racers`. With a screenshot, `ticks` race steps at full
 * throttle first. */
int track_viewer_drive(const char *data_root, const char *name, int model, int racers, int race_type, uint32_t ticks,
                       const char *screenshot);

/* The front end's race (glue standing in for what 00427ae0 / 004275f8 take
 * from config.dat): the player's gearbox and aids, the laps, the head to
 * head opponent, the music and the volumes; the race runs in the menus'
 * window (not made or closed here) and, once over, returns at once instead
 * of waiting for Escape. Set before each track_viewer_drive from the menus;
 * the next drive takes it once. */
typedef struct {
    int automatic;      /* 00524e87 0 */
    int anti_lock;      /* 00524e89 1 */
    int traction;       /* 00524e7f 1 */
    int laps;           /* 00524ea1, 0 the drive's 2 */
    int opponent;       /* 00524e81: the head to head rival's car */
    int music;          /* 00524e63: the music setting, 0 none */
    int music_volume;   /* the short 00524e65 */
    int sound_volume;   /* the short 00524e69 */
    int sound_mode;     /* the short 00524e61 */
    uint8_t *game_state;  /* 0052ad44 (NULL: none): the race starts on its first 0x5b6 bytes and writes its results back */
} TrackViewerMenus;
void track_viewer_drive_menus(const TrackViewerMenus *menus);

/* The last drive's end: over once the race ended (race_sim_over), with
 * 00509d6c and the results screen's words 0051dcbc..0051dccb and
 * 004f82a0 as 0045af80 / 0045b29c left them. */
typedef struct {
    int over;
    int16_t end_request;  /* 00509d6c: 1 finished, 0x10 out */
    int16_t place, rival_place, rival, rival_car;  /* 0051dcc2, 0051dcc0, 0051dcc6, 0051dcc8 */
    int16_t dropped[8];   /* 004f82a0 */
} TrackViewerResults;
TrackViewerResults track_viewer_drive_results(void);

#endif
