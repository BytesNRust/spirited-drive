/* The race globals the car simulation reads and writes, gathered in one
 * place. Each field names the original's address; tools/probe_car.c maps them
 * back for the oracle. */
#ifndef TNFS_RACE_STATE_H
#define TNFS_RACE_STATE_H

#include "formats/tri.h"
#include "game/race/ai_struct.h"
#include "game/race/car.h"
#include "game/race/crash_trail.h"

#include <stdint.h>

enum {
    RACE_NOTES = 0x7080 / 0x3c,   /* 00512f94: one per second of a 30 minute clock */
    RACE_TRACK_NAME = 16,
    RACE_PLAYERS = 8,
    RACE_LAP_MARKS = 109,         /* what fits in a 0x1d2-byte player record before the police stop counts */
    RACE_SURFACES = 3
};

/* 004fa9b0 + 16 * surface (car +0x4a2), set per track by 0042f734. */
typedef struct {
    int32_t grip;       /* 8.8 */
    int32_t rolling;    /* 8.8, times the car's drag */
    int32_t drag;       /* constant resistance */
    int32_t off_road;   /* copied to car +0x4a6 (4 when set) */
} Surface;

/* 0051cc54 + 0x1d2 * player: the start tick, then the tick each lap ended,
 * and what happened to the player in the race. */
#pragma pack(push, 1)
typedef struct {
    int32_t unknown_000;
    int32_t lap_marks[RACE_LAP_MARKS];  /* +4: [0] start, [k] end of lap k */
    int32_t police_stops[2];  /* +0x1b8, +0x1bc (0051ce0c, 0051ce10): the police stop's two outcomes (0046bb78) */
    int32_t crashes;        /* +0x1c0 (0051ce14): hard crashes while racing (0040e050) */
    int32_t unknown_1c4[2];
    int32_t top_speed;      /* +0x1cc: the fastest forward speed, 16.16 (car_physics RECORD_TOP_SPEED; the computer cars, 0046f3e8) */
    uint8_t unknown_1d0[2];
} PlayerRecord;
#pragma pack(pop)
typedef char player_record_size_check[sizeof(PlayerRecord) == 0x1d2 ? 1 : -1];

/* 004fb7fa + 0x4b * slot: who drives the car in a slot and how. */
#pragma pack(push, 1)
typedef struct {
    char name[9];
    int32_t car;          /* +0x09: 004bc664 names, "TSupra" first */
    int32_t automatic;    /* +0x0d */
    int32_t traction;     /* +0x11: traction control wanted */
    int32_t anti_lock;    /* +0x15: ABS wanted */
    uint8_t unknown_19[0x32];
} RaceEntry;
#pragma pack(pop)
typedef char race_entry_size_check[sizeof(RaceEntry) == 0x4b ? 1 : -1];

typedef struct {
    void *user;
    /* 00443eac: a sound effect (arguments as the original's; 00444028 and
     * 00443fc0 lead here) */
    void (*sound)(void *user, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);
    /* 004646cc + 00464850: a voice sample, "besttime.eas" and the like */
    void (*speech)(void *user, const char *name);
    /* 004acd8a: a tone, the gear change (0043cda0) or a wall scrape (0043cd58) */
    void (*tone)(void *user, int32_t a, int32_t b, int32_t c);
    /* 0046a654 on cars 1.. of 00509e1c (0046b080): pushes from a blown engine.
     * TODO(0046a654): the push itself, once the cars collide. */
    void (*push)(void *user, int32_t car, int32_t x, int32_t y, int32_t z);
    /* 0046a654 from a wall (004302a4) on the record car +0x88 names.
     * TODO(0046a654): the crash physics. */
    void (*crash)(void *user, uint32_t target, int32_t x, int32_t y, int32_t z);
    /* 0041019c: while a recording plays, the followed car is still on the
     * start line (00440afc). */
    void (*start_line)(void *user, int32_t player);
    /* 0044435c: the followed car's engine sounds stop (a crash) */
    void (*stop_engine)(void *user);
    /* 00444394: the followed car's engine sounds start again */
    void (*start_engine)(void *user);
    /* 0043d120 (1, ...): a knock felt through the controls; z speed and kick */
    void (*buffet)(void *user, int32_t speed, int32_t kick);
    /* 00404530: the camera attaches to car `slot` in `mode` (a computer car
     * placed while the race has not started takes the view, 0046f110). */
    void (*camera)(void *user, int32_t slot, int32_t mode);
    /* 0043cc84: the force-feedback jolt of a car-to-car hit felt by the followed
     * car (_Jolt): the two cars' slots, the push along x, y and z, the impulse. */
    void (*jolt)(void *user, int32_t car_a, int32_t car_b, int32_t x, int32_t y, int32_t z, int32_t impulse);
} RaceSounds;

/* A computer driver's profile (0x8d bytes at *0050bae0 + 0x8d * letter,
 * 0044941c; 00449400 points 0050bae0 at 004c2888, where profiles 0..8 are).
 * Only what the computer cars' code reads is known. */
#pragma pack(push, 1)
typedef struct {
    uint8_t unknown_00[0x65];
    int32_t lane_speed_limit; /* +0x65: the fastest sideways speed toward a lane (0046f354) */
    int32_t lane_speed_rate;  /* +0x69: that speed per unit of forward speed */
    uint8_t unknown_6d[0x0c];
    int32_t lane_bias[4];   /* +0x79: one is picked at random as car +0x33c (00470a34) */
    int32_t reset_odds;     /* +0x89: how often a car's bias is picked again (00470028), out of 0x10000 */
} DriverProfile;
#pragma pack(pop)
typedef char driver_profile_size_check[sizeof(DriverProfile) == 0x8d ? 1 : -1];

/* 004fb724 + 12 * player: how a player drives. */
typedef struct {
    int32_t analog_steering;  /* 0: keys (00438954), else a wheel or stick (00438aac) */
    int32_t pedals;           /* 1: analog (00438ff0), else keys (00438f00) */
    int32_t unknown_8;
} ControlSetup;

typedef struct {
    const Tri *tri;                 /* 00505038 nodes, 00504208 mask (ring_nodes - 1) */
    char track_name[RACE_TRACK_NAME];  /* 004fb7c6, "tr6" */
    int32_t track;                  /* 004fb7c2: 0..7 */
    int32_t track_variant;          /* 004fb7be */
    int32_t arcade;                 /* 004fbadc */
    int32_t game_mode;              /* 004c5d28: 4 makes blown engines push the cars */
    int32_t car_count;              /* 004fb7e8 */
    Surface surfaces[RACE_SURFACES];  /* 004fa9b0 */
    uint32_t track_flags;           /* 004fb7d8 */
    int32_t player;                 /* 004fbb08: the car the camera follows */
    int32_t phase;                  /* 004c5390: 1 while racing */
    int32_t tick;                   /* 00513e70 */
    int32_t clock;                  /* 00512d90: tick % 0x7080 */
    int32_t last_node;              /* 004fa18c */
    int32_t landing_shake;          /* 004f9b74: frames of camera shake after a jump */
    /* 0044d744: the loudest note of each second, (second << 8) | (0x80 + level) */
    int32_t note_second;            /* 00513714 */
    int32_t note_tick;              /* 004c53bc */
    int32_t note_level;             /* 004c53c0 */
    int32_t camera_hold;            /* 00513e74: ticks the camera keeps its view (60 from 0045573c) */
    int32_t stop_notice[2];         /* 004b92dc, 004b92e0: the police stop's two outcomes on screen (0041e370, 0041e3a8) */
    int32_t stop_notice_tick;       /* 004b92e4: the tick they went off; -1000 at the race start */
    int32_t unknown_4c538c;         /* 004c538c: 1 when a held car drives again (00470e70) or a crashed one (0040e050) */
    /* a crash sounds only when louder than the last one, which fades
     * (0040d8e8: 0x6666 once the ticks run out) */
    int32_t crash_sound_level;      /* 004b9094: the last crash sound's impact + 0x8000 */
    int32_t crash_sound_ticks;      /* 004b9090: 10 after a crash sound, counting down */
    int32_t unknown_4b91c0;         /* 004b91c0: 0x1e when a player's crash ends while racing (0040e050) */
    int32_t rand_state[6];          /* 004c6f90..004c6fa4: 00487c2c's add-with-carry generator, top word first */
    int32_t car_impulse;            /* 004b9128: the last push between two cars' bodies (0041c098), kept when they part */
    int32_t unknown_509d5c;         /* 00509d5c: 0 from 0043f994, 1 from 0043fbac; a crash is not counted at 2 or 3 */
    CrashTrail trail;               /* 005146ac: the crash smoke */
    uint32_t trail_seed;            /* 004c5608: its own generator */
    int32_t notes[RACE_NOTES];      /* 00512f94 */
    int32_t circuit;                /* 00504200: laps are counted */
    int32_t segments;               /* 0050420c */
    int32_t loop_nodes;             /* 00509d58: nodes in one lap */
    int32_t laps;                   /* 00509d54 */
    int32_t best_lap;               /* 00509e04: ticks */
    int32_t best_lap_set;           /* 00509dfc */
    int32_t viewed;                 /* 004fbb04: the car shown in a replay */
    int16_t replay;                 /* 0052b316 */
    int16_t playback;               /* 0052b312: non-zero while a recording drives the race */
    int32_t humans;                 /* 004fb7e0: players at the controls */
    /* the start: holding the throttle keys for a revved launch (0042f138) */
    int32_t start_clock;            /* 004bb8e0 */
    int32_t start_input;            /* 004faa50: the player's start keys are read */
    uint8_t start_keys[4];          /* 004c696d, 004c6966, 004c6972, 004c696c */
    int32_t launch;                 /* 004badd4: a launch is under way */
    int32_t launch_full_until;      /* 004badcc */
    int32_t launch_end;             /* 004badd0 */
    int32_t launch_ready[RACE_PLAYERS];  /* 004faa5c: the other players' launches */
    int32_t wheelspin;              /* 004bad40: the drive force broke traction this tick */
    int32_t tyre_force_sum;         /* 00509e0c: the last tyre call's two forces added */
    int16_t effects;                /* 0052b320: wall scrape tones on */
    int32_t default_surface;        /* 004b95c0: between the verges */
    int32_t views;                  /* 004f801c: more than one pans the sounds by position */
    int32_t listener[3];            /* 004f802c */
    int32_t listener_heading;       /* 004f8060 */
    int32_t crash_spin;             /* 004bad18 */
    PlayerRecord players[RACE_PLAYERS];  /* 0051cc54 */
    Car *cars[RACE_PLAYERS];        /* 00509e1c: the cars by slot */
    int32_t by_node[RACE_PLAYERS];  /* 00509e68: slots by road node, the rearmost first (00440dd8) */
    int32_t by_progress[RACE_PLAYERS];  /* 00509e44: slots by progress, last place first (00440dd8) */
    const Car *leader;              /* 00509e18: the car in slot 0, once loaded */
    const Car *second_car;          /* 00509e40: the car in slot 1, once entered */
    RaceEntry entries[RACE_PLAYERS];  /* 004fb7fa */
    int32_t specs_loaded;           /* 004bc698: the specs are in memory already, as tuned */
    /* 004fb704: each player's controls this tick: steering in bits 0..5
     * (0x20 centre), the pedal in 6..11 (0x1e at rest, down brakes), then
     * gear up 0x1000, gear down 0x2000, look back 0x4000, handbrake 0x8000 */
    uint32_t inputs[RACE_PLAYERS];
    ControlSetup controls[RACE_PLAYERS];  /* 004fb724 */
    /* the computer cars */
    AiState ai;                     /* 0052b330 */
    /* ai.profiles[slot] holds the original's address of a driver profile;
     * these map those addresses to the profiles the port holds. */
    const DriverProfile *driver_profiles;
    uint32_t driver_profiles_base;  /* the address of driver_profiles[0] in the original */
    int32_t driver_profile_count;
    int32_t grid[RACE_PLAYERS];     /* 004fba50: the starting grid place of each car */
    int32_t camera_mode;            /* 004f8020: 7 while the race is being set up, 5 attached to a car */
    uint32_t random_seed;           /* 004b9628: race_random() */
    uint32_t random_last;           /* 004b962c */
    uint32_t random_mul;            /* 004b9630 */
    int32_t unknown_4fbab8;         /* non-zero: 0046e480 ends the police chase first */
    int32_t racers;                 /* 004fb7dc: cars in the race, humans first */
    int32_t cops;                   /* 004fb7e4: police cars after the racers */
    int32_t traffic_setting;        /* 004fb7ec */
    int32_t unknown_4fb7f0;         /* 004fb7f0 */
    int32_t default_profile_odds;   /* 004c5d20 */
    int32_t unknown_4c5cd8, unknown_4c5cdc, unknown_4c5d40;
    uint8_t tick_flags;             /* 004fcdb0: 4 runs the engine and the notes, 0x10 the driver aids */
    int32_t catch_up;               /* 004fbac0: two players: the trailing car runs this many ticks a second more at most */
    int32_t first_move_tick;        /* 00509d64: the tick after the car first moved, 0 before */
    /* 004bae70..: the arcade steering's turn rate (00430cb0); the road
     * assist table reads on into them at high speed */
    int32_t arcade_turn;            /* 004bae70 */
    int32_t arcade_grip;            /* 004bae74 */
    int32_t arcade_turn_limit;      /* 004bae78 */
    /* the drag strip of track 3 (its first 0x61 and last nodes): times for
     * the cockpit dials and the player records */
    int32_t race_type;              /* 004fb7d0 (00524e5d): 0 time trial, the only one timing the strip */
    int32_t strip_brake_80_tick;    /* 004bae7c: last tick above 80 mph braking */
    int32_t strip_brake_60_tick;    /* 004bae80 */
    int32_t strip_best_60;          /* 004bae84: 0-60 ticks this run */
    int32_t strip_best_100;         /* 004bae88 */
    int32_t strip_best_quarter;     /* 004bae8c */
    int32_t strip_start_tick;       /* 004bae90: the car was last nearly still */
    int32_t strip_start_progress;   /* 004bae94 */
    /* dashboard readings the simulation leaves for the cockpit view */
    int32_t gauge_acc, gauge_ac8;   /* 004f9acc, 004f9ac8 (0041e858) */
    int32_t dash_ab8, dash_abc, dash_ac0, dash_ac4;  /* 004f9ab8.. (0041e7fc) */
    RaceSounds sounds;
} RaceState;

enum {
    /* 004fb7d8 bit 0x20: falling cars skip the extra half pull (00428bd8) and,
     * except on "tr6", come back from long jumps more slowly */
    TRACK_FLAG_LOW_GRAVITY = 0x20
};

/* The car whose record holds `handle` at +0x88 (the original's pointer to
 * the car, see car_reset), or NULL. */
Car *race_car_by_handle(const RaceState *race, uint32_t handle);

/* 00440dd8: sorts by_node and by_progress (ties by the lateral +0x39d). */
void race_sort_cars(RaceState *race);

/* 004b9628 * 004b9630: the next random value, 0..0xffff */
uint32_t race_random(RaceState *race);

/* 00487c2c: the next of the add-with-carry generator: each word added to the
 * next with carry, the lowest counting up, the top word the result. */
uint32_t race_rand(RaceState *race);

/* 0044d744 */
void race_note(RaceState *race, int32_t level);

/* 0042f734 */
void race_surfaces_init(RaceState *race);

/* 0046b080 */
void race_blow_up(RaceState *race);

/* 004bad20 + 4 * (gear + 2): how fast revs and wheels close on each other
 * per gear. The table holds gears -2..5; sixth gear reads the next word,
 * which is the wheelspin flag (004bad40). */
int32_t race_gear_step(const RaceState *race, int32_t gear);

/* 0041e858 and 0041e7fc: values above 500 (1200) are ignored. */
void race_dash_gauges(RaceState *race, int32_t a, int32_t b);
void race_dash_values(RaceState *race, int32_t a, int32_t b, int32_t c, int32_t d);

/* The road node under a node index, masked like the original (& 00504208). */
TriNode race_node(const RaceState *race, int32_t node);

#endif
