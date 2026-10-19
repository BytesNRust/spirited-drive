/* Development track viewer (see track_viewer.c). */
#ifndef TNFS_TRACK_VIEWER_H
#define TNFS_TRACK_VIEWER_H

#include "game/race/input_queue.h"
#include "game/race/race_state.h"

#include <stddef.h>
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

/* A replay file (.RPL, 0044d420's) played back as --drive: its setup's race (track, cars, laps, the seed) started
 * as 004275f8 starts a replay (0052b316 set, 0044f534(4): 0044d514 reads the file at the first tick and the
 * replay plays from the start); 0 when the file is not a replay. */
int track_viewer_drive_replay(const char *data_root, const char *path);
/* The same on a replay's bytes (REPLAY_FILE_SIZE, the caller's) and 004275f8's request for its kind (00524e9f 1:
 * 4, from the start; 2: 5, the highlights); in the menus' window when track_viewer_drive_menus came first */
int track_viewer_drive_replay_data(const char *data_root, const uint8_t *file, size_t size, int request);

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
    int opponent;       /* the head to head rival's car model: 00524e81's pick through 00426620 */
    int music;          /* 00524e63: the music setting, 0 none */
    int music_volume;   /* the short 00524e65 */
    int sound_volume;   /* the short 00524e69 */
    int sound_mode;     /* the short 00524e61 */
    uint8_t *game_state;  /* 0052ad44 (NULL: none): the race starts on its first 0x5b6 bytes and writes its results back */
    int time_of_day;    /* 004fbae0[track]: 0 morning, 1 midday, 2 evening (004278c0 takes 1 with a variant) */
    int variant_m;      /* 004fbad8: the track's "m" art */
    int variant_r;      /* 004fbadc: the track's "r" art */
} TrackViewerMenus;
void track_viewer_drive_menus(const TrackViewerMenus *menus);

/* A link game's race (the front end's glue): the humans race each other's
 * cars, this machine at its slot, the inputs going round as the original
 * sends them (00445e00) and coming in through track_viewer_drive_message.
 * Set before a track_viewer_drive, which takes it once; none races alone. */
typedef struct TrackViewerLink {
    int32_t humans;                     /* 004fb7e0 (004f81d2): the machines' cars, first in the field */
    int32_t slot;                       /* 004fbb08: this machine's car */
    int32_t host;                       /* 004fbb0c */
    /* 004275f8's link start: 00410c18(2), 00410d80 (004fbac8, the queue's wait, to *wait_ticks), then 004273d0:
     * this machine's entry (its name filled in) to the host, and the host's setup back: each slot's entry at
     * 004fb7fa + 0x4b x slot (a RaceEntry apart), the humans' names to `names` (0052ad44, NULL: none kept);
     * *seed is 004fb7f4, written to the setup before (this machine's time) and read back after (the host's) */
    const uint8_t *(*exchange)(const RaceEntry *own, uint8_t *names, int32_t *wait_ticks, uint32_t *seed);
    void (*send)(int32_t to, const int16_t *words, int32_t size);  /* 00445e00 */
    void (*service)(void);              /* 00475c14(0): the transport's */
    int32_t (*wait)(int32_t stage);     /* 00410c18: nonzero when all came */
    int16_t (*slot_of)(uint32_t link);  /* 0044659c */
    void (*sync)(void);                 /* 004104d0 */
    void (*host_ready)(void);           /* 00446284 */
    void (*deadline)(int32_t ticks);    /* 00410b7c */
    int16_t (*any_ready)(void);         /* 004109b4 */
    void (*chat)(void);                 /* 0040a5d8 */
    int16_t *stage_flag;                /* 0050a5b6, the link's: the stage the host sent */
    int16_t *lost;                      /* 0052b314, the link's: the game dropped */
    int16_t *stand_ins;                 /* 004f829e, the link's: the kind word, then each player's stand-in */
    void (*drop)(int16_t player);       /* 004463ec */
    /* While the race runs: its queues (004faa7c), records (0051cc54) and 0041019c, for the link's own handlers
     * (004460cc's lost link, 004463ec, kind 10); all NULL after */
    void (*racing)(InputQueue *queues, PlayerRecord *records, void (*stand_in)(int16_t player));
} TrackViewerLink;
void track_viewer_drive_link(const TrackViewerLink *link);

/* The race's messages from a link while a link race runs: kind 2 the inputs
 * (004395cc), kind 3 the schedule (004109e4) */
void track_viewer_drive_message(int16_t kind, uint32_t link, const uint8_t *data, int32_t size);

/* The last drive's end: over once the race ended (race_sim_over), with
 * 00509d6c and the results screen's words 0051dcbc..0051dccb and
 * 004f82a0 as 0045af80 / 0045b29c left them. */
typedef struct {
    int over;
    int16_t end_request;  /* 00509d6c: 1 finished, 0x10 out */
    int16_t place, rival_place, rival, rival_car;  /* 0051dcc2, 0051dcc0, 0051dcc6, 0051dcc8 */
    int16_t dropped[8];   /* 004f82a0 */
    int16_t player_place, shown_place;  /* 0051dcc4, 0051dcbe */
    int32_t racers;       /* 004fb7dc */
    int32_t player;       /* 004fbb08 */
    int32_t end_state;    /* 00509d70: 0x40 a replay of 00524e9f 3 left by its viewer (004275f8 answers 1) */
} TrackViewerResults;
TrackViewerResults track_viewer_drive_results(void);

#endif
