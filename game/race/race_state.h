/* The race globals the car simulation reads and writes, gathered in one
 * place. Each field names the original's address; tools/probe_car.c maps them
 * back for the oracle. */
#ifndef TNFS_RACE_STATE_H
#define TNFS_RACE_STATE_H

#include "formats/tri.h"
#include "game/race/ai_struct.h"
#include "game/race/car.h"
#include "game/race/chase_struct.h"
#include "game/race/crash_trail.h"
#include "game/race/race_splits.h"

#include <stddef.h>
#include <stdint.h>

enum {
    RACE_NOTES = 0x7080 / 0x3c,   /* 00512f94: one per second of a 30 minute clock */
    RACE_TRACK_NAME = 16,
    RACE_PLAYERS = 8,
    RACE_CARS = 9,                /* 00509e1c, 00509e44, 00509e68: racers, police and traffic (00439c98) */
    RACE_LAP_MARKS = 109,         /* what fits in a 0x1d2-byte player record before the police stop counts */
    RACE_SURFACES = 3,
    RACE_VOICES = 16,             /* 0050a4cc: 8 bytes each, up to the banks at 0050a54c */
    REPLAY_CUTS = 40,             /* 00512d94..00512f74 */
    REPLAY_PICKS = 30             /* 0044d7d8: at most 30 cuts; its picks fill the last 10 rows */
};

/* 00512d94 + 12 * i: a cut of the replay's director (0044fc74 steps through
 * them): the clock it jumps to, the clock it plays until (0044e700) and the
 * camera's word (0044eb34). */
typedef struct {
    int32_t clock, until, word;
} ReplayCut;

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
    int32_t finish;         /* +0x1c4 (0051ce18): the tick - 300 the car finished, -999 out before the start, 0 still racing (00440c30) */
    int32_t unknown_1c8;
    int32_t top_speed;      /* +0x1cc: the fastest forward speed, 16.16 (car_physics RECORD_TOP_SPEED; the computer cars, 0046f3e8) */
    uint8_t unknown_1d0[2];
} PlayerRecord;
#pragma pack(pop)
typedef char player_record_size_check[sizeof(PlayerRecord) == 0x1d2 ? 1 : -1];

/* 004fb724 + 12 * player: how a player drives. */
typedef struct {
    int32_t analog_steering;  /* 0: keys (00438954), else a wheel or stick (00438aac); 0043915c: 1 the pad, 2.. the mouse */
    int32_t pedals;           /* 1: analog (00438ff0), else keys (00438f00); 0043915c: 3 the pad's buttons, 2 the mouse */
    int32_t gears;            /* 0043915c: 0 keys, 3 the pad's buttons, else the mouse's */
} ControlSetup;

/* 004fb7fa + 0x4b * slot: who drives the car in a slot and how. */
#pragma pack(push, 1)
typedef struct {
    char name[9];
    int32_t car;          /* +0x09: 004bc664 names, "TSupra" first */
    int32_t automatic;    /* +0x0d */
    int32_t traction;     /* +0x11: traction control wanted */
    int32_t anti_lock;    /* +0x15: ABS wanted */
    uint8_t unknown_19[4];
    int32_t sound_volume; /* +0x1d (004fb817): the sound volume, read for the followed slot; 0 plays no car sounds (0044404c) */
    int32_t music_volume; /* +0x21 (004fb81b): 00464604's (the music's) volume, read for the followed slot */
    int32_t sound_mode;   /* +0x25 (004fb81f): 2 plays the followed car's engine unpanned (004444b8) */
    int32_t sound_on;     /* +0x29 (004fb823): non-zero, the race loads its sound banks for the followed slot (00443924) */
    uint8_t unknown_2d[4];
    ControlSetup controls;  /* +0x31 (004fb82b): the player's 004fb724 (004275f8 puts it here from 00426ddc's, so the
                             * replay's setup keeps it, and takes every human's back out after the setup is read) */
    uint8_t unknown_3d[0xe];
} RaceEntry;
#pragma pack(pop)
typedef char race_entry_size_check[sizeof(RaceEntry) == 0x4b ? 1 : -1];
typedef char race_entry_controls_check[offsetof(RaceEntry, controls) == 0x31 ? 1 : -1];

typedef struct {
    void *user;
    /* 00443eac: a sound effect (arguments as the original's; 00444028 and
     * 00443fc0 lead here) */
    void (*sound)(void *user, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);
    /* 004646cc + 00464850: a voice sample, "besttime.eas" and the like */
    void (*speech)(void *user, const char *name);
    /* 004acd8a: a tone, the gear change (0043cda0) or a wall scrape (0043cd58) */
    void (*tone)(void *user, int32_t a, int32_t b, int32_t c);
    /* 0046a654 on cars 1.. of 00509e1c (0046b080): pushes from a blown engine
     * (race_sim: car_push_spin). */
    void (*push)(void *user, int32_t car, int32_t x, int32_t y, int32_t z);
    /* 0046a654 from a wall (004302a4) on the record car +0x88 names
     * (race_sim: car_push_spin). */
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
    /* 004acd84 (_XYVectorForce): the steady force on the controls, each of
     * x and y in -100..100 (0043ce08); NULL for none. */
    void (*force)(void *user, int32_t x, int32_t y);
    /* The sound driver's voices: 0048eea0 starts `sound` (pan, then volume
     * and pitch as bytes) and returns the voice, negative when it could not;
     * 00473b4c, 00473c08 and 00473bb0 set a playing voice's volume (0..0x7f),
     * pitch (0..0x7f) and pan (0..0xff); 00473d20 is non-zero once the voice
     * has stopped. NULL: no voice starts, and a voice counts as stopped. */
    int32_t (*voice_start)(void *user, int32_t sound, int32_t pan, uint8_t volume, uint8_t pitch);
    void (*voice_volume)(void *user, int32_t voice, int32_t volume);
    void (*voice_pitch)(void *user, int32_t voice, int32_t pitch);
    void (*voice_pan)(void *user, int32_t voice, int32_t pan);
    int32_t (*voice_stopped)(void *user, int32_t voice);
    /* 0043d120 (0): with 004fcdd4 set, 0043cbf4 lets the controls' force
     * go (00444394, the wall tones on); NULL for none. */
    void (*force_release)(void *user);
    /* 004acd72 / 004acd6c (_XVibration, _YVibration; axis 0 / 1): the
     * controls shake with three words (cdecl, as the exe passes them);
     * 004acd7e / 004acd78 (_X / _YVibrationClear) stop it. NULL for none. */
    void (*vibration)(void *user, int32_t axis, int32_t a, int32_t b, int32_t c);
    void (*vibration_clear)(void *user, int32_t axis);
    /* 00473c68: a playing voice stopped; NULL for none. */
    void (*voice_stop)(void *user, int32_t voice);
    /* 004acd90 / 004acd96 / 004acd9c (_EnableForces, _DisableForces,
     * _ClearForces): the controls' forces; NULL for none. */
    void (*forces_enable)(void *user);
    void (*forces_disable)(void *user);
    void (*forces_clear)(void *user);
    /* 004acd66 (_Buffeting, cdecl level): the controls buffeted (0043d120
     * gives 0x50); NULL for none. */
    void (*buffeting)(void *user, int32_t level);
} RaceSounds;

/* 0050a4cc + 8 * channel: the voice a sound channel plays and its sound. */
/* The followed car's engine sounds (004444b8), 004bcb6c..004bcb93. */
typedef struct {
    int32_t unknown_6c;         /* 004bcb6c: 0x7f as the exe starts */
    int32_t channel;            /* 004bcb70: its low 16 bits, the voices[] channel of the engine's second voice (7) */
    int32_t pulse_armed;        /* 004bcb74: 00444230's countdown */
    int32_t pulse_count;        /* 004bcb78 */
    int32_t pitch;              /* 004bcb7c: the road noise's pitch (voices[5]) */
    int32_t revs;               /* 004bcb80: 0..0x7f the engine's pitch */
    int32_t throttle;           /* 004bcb84: the throttle eased */
    int32_t rising;             /* 004bcb88: the road noise's pitch swings up (1) or down */
    int32_t node_volume;        /* 004bcb8c: the node sound on channel 0xb, up and down by 5 */
    int32_t node_pan;           /* 004bcb90 */
} RaceEngineSound;

typedef struct {
    int32_t voice;
    int32_t sound;
} RaceVoice;

/* A computer driver's profile (0x8d bytes at *0050bae0 + 0x8d * letter,
 * 0044941c; 00449400 points 0050bae0 at 004c2888, where profiles 0..8 are).
 * Only what the computer cars' code reads is known. */
#pragma pack(push, 1)
typedef struct {
    uint8_t unknown_00;
    int32_t pace_by_gap[21];  /* +1: the target speed's scale by the gap to the first car (00470a9c) */
    int32_t mass_scale;       /* +0x55: scales the car's +0x170 in its target speed */
    int32_t bend_scale;       /* +0x59: scales its pace through bends */
    int32_t block_reach;      /* +0x5d: nodes ahead of a human within which the car leans into the human's lane (0046bfb0) */
    int32_t block_gap;        /* +0x61: how far the human must be behind along the road, 16.16 */
    int32_t lane_speed_limit; /* +0x65: the fastest sideways speed toward a lane (0046f354) */
    int32_t lane_speed_rate;  /* +0x69: that speed per unit of forward speed */
    int32_t oncoming_reach;   /* +0x6d: how many nodes ahead a racer counts an oncoming car (0046b0d8) */
    uint8_t unknown_71[0x08];
    int32_t lane_bias[4];   /* +0x79: one is picked at random as car +0x33c (00470a34) */
    int32_t reset_odds;     /* +0x89: how often a car's bias is picked again (00470028), out of 0x10000 */
} DriverProfile;
#pragma pack(pop)
typedef char driver_profile_size_check[sizeof(DriverProfile) == 0x8d ? 1 : -1];

typedef struct {
    const Tri *tri;                 /* 00505038 nodes, 00504208 mask (ring_nodes - 1) */
    char track_name[RACE_TRACK_NAME];  /* 004fb7c6, "tr6" */
    int32_t track;                  /* 004fb7c2 (00524e85): 0..8 (race_sim_track_index) */
    int32_t track_variant;          /* 004fb7be */
    int32_t arcade;                 /* 004fbadc */
    int32_t game_mode;              /* 004c5d28: 4 makes blown engines push the cars */
    int32_t car_count;              /* 004fb7e8 */
    Surface surfaces[RACE_SURFACES];  /* 004fa9b0 */
    uint32_t track_flags;           /* 004fb7d8 */
    int32_t player;                 /* 004fbb08: the car the camera follows */
    int32_t phase;                  /* 004c5390: 1 while racing */
    int32_t tick;                   /* 00513e70 */
    int32_t unknown_513e60;         /* 00513e60: 0045597c runs the tick only while 0 */
    int32_t unknown_513e64;         /* 00513e64: 0045597c runs the tick only while set */
    int32_t unknown_4c5398;         /* 004c5398: with phase 2 and 0, the tick count stands (0045597c) */
    int32_t unknown_4c539c;         /* 004c539c: counted down by 0045597c while above 0 */
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
    RaceSplits splits;              /* 0051c808 */
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
    int32_t port_nitro_start;       /* port: 1 the launch takes N alone or the pad's nitro key (start_keys.h), 0 the exe's
                                     * N and a shift or B */
    int32_t launch;                 /* 004badd4: a launch is under way */
    int32_t launch_full_until;      /* 004badcc */
    int32_t launch_end;             /* 004badd0 */
    int32_t launch_ready[RACE_PLAYERS];  /* 004faa5c: the other players' launches */
    int32_t wheelspin;              /* 004bad40: the drive force broke traction this tick */
    int32_t tyre_force_sum;         /* 00509e0c: the last tyre call's two forces added */
    int16_t effects;                /* 0052b320: the force-feedback stick open (0043cb50), its effects on */
    int32_t default_surface;        /* 004b95c0: between the verges */
    /* 004f801c, the follow camera: kind above 1 pans the sounds by position,
     * position (004f802c) and angles[1] (004f8060) are the listener, node
     * (004f8054) the camera's node (scenery hits within 9 nodes of it sound),
     * flags (004f8058) bit 0 the scenery must be streamed again */
    RaceChase chase;
    ChaseKind chase_kinds[CHASE_KINDS];  /* 004b6bb0 (race_chase_init) */
    RaceCamera cameras[RACE_CAMERAS];    /* 004b78a8 */
    int32_t window_size;            /* 00511ff0: picks a view row's layout */
    int32_t camera_pitch;           /* 004f9b70: added to the in-car view's pitch (0041f9a0, 0042270c) */
    int32_t camera_tilt;            /* 004b91cc: 0x10000, the share of the car's tilt the in-car view takes */
    int32_t camera_roll_add;        /* 004c53a8: added to the roll as placed when the kind is above 1 */
    int32_t view_taken;             /* 0052b8f8: set when a kind is taken (00405b00) */
    int32_t chase_auto;             /* 004b7a28: 1 at the start; 0040594c's turn to view 6 or 7 needs it and
                                     * clears it, taking view 5 there sets it again */
    int32_t unknown_513e6c;         /* 00513e6c: the phase as 00440ef0 saw it; with replay, kinds 0 and 10 become 1 and 11 */
    /* 00405210 on the camera in use (race_view_select): the view and the
     * window size; NULL for none */
    void (*select_view)(void *user, RaceCamera *camera, int32_t view, int32_t size);
    void *view_user;
    int32_t crash_spin;             /* 004bad18 */
    PlayerRecord players[RACE_PLAYERS + 1];  /* 0051cc54; the 9th takes the place kept one on (00440c30) and the last car's model (0045af80) */
    Car *cars[RACE_CARS];        /* 00509e1c: the cars by slot */
    int32_t by_node[RACE_CARS];  /* 00509e68: slots by road node, the rearmost first (00440dd8) */
    Car *drawn[RACE_CARS];       /* 00509e8c: the cars drawn, by_node's order (004414e0) */
    int32_t drawn_count;         /* 00509e14 */
    int32_t by_progress[RACE_CARS];  /* 00509e44: slots by progress, last place first (00440dd8) */
    uint8_t car_places[RACE_CARS];  /* 004f9e83: each slot's place, 1 first (0041f434) */
    int16_t start_beeps[6];         /* 00513e78..00513e82: one a start light, cleared when it lights (0041ec88) */
    const Car *leader;              /* 00509e18: the car in slot 0, once loaded */
    const Car *second_car;          /* 00509e40: the car in slot 1, once entered */
    RaceEntry entries[RACE_PLAYERS];  /* 004fb7fa */
    int32_t specs_loaded;           /* 004bc698: the specs are in memory already, as tuned */
    /* 004fb704: each player's controls this tick: steering in bits 0..5
     * (0x20 centre), the pedal in 6..11 (0x1e at rest, down brakes), then
     * gear up 0x1000, gear down 0x2000, horn 0x4000, handbrake 0x8000 */
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
    int32_t driver_letters[RACE_PLAYERS];  /* 004fba70: each opponent's driver profile number (0044940c) */
    uint32_t random_seed;           /* 004b9628: race_random() */
    uint32_t random_last;           /* 004b962c */
    uint32_t random_mul;            /* 004b9630 */
    int32_t unknown_4fbab8;         /* non-zero: 0046e480 ends the police chase first */
    int32_t models[RACE_CARS];      /* 004fba94: each slot's car model (00457870); see race_models.h */
    int32_t mirrored;               /* 004fbad8: the "m" tracks; 00457d40 flips bit 0x200 of every car polygon */
    int32_t wheel_tick;             /* 0051c7f4: the tick 00458614 last turned the wheels on */
    int32_t wheel_counter;          /* 004c5810: 00458614's 0 / 1 step */
    int32_t wheel_toggle;           /* 004c5814: frames 1 and 2 in turn below 0xa0000 */
    int32_t wheel_last_x, wheel_last_y;  /* 004c5818, 004c581c: the followed car's x, y at the last call */
    uint8_t loading_used[20];       /* 0051c7de: the loading pictures shown this race (004571e8) */
    uint8_t unknown_4fb787;         /* 004fb787: non-zero puts the other humans and opponents in the 100 set (00457870) */
    int32_t unknown_4fbad0;         /* 004fbad0: the head to head opponent pick, 00426fec of its low half (00457870) */
    int32_t racers;                 /* 004fb7dc: cars in the race, humans first */
    int32_t cops;                   /* 004fb7e4: police cars after the racers */
    int32_t traffic_setting;        /* 004fb7ec */
    int32_t unknown_4fb7f0;         /* 004fb7f0 */
    int32_t default_profile_odds;   /* 004c5d20 */
    int32_t unknown_4c5cd8, unknown_4c5cdc;
    int32_t unknown_4c5cd4;         /* 004c5cd4: with one human, the manager may spin out a car near it while it sounds the horn (0046c490); set and cleared by 00432ff4 */
    int32_t unknown_509d60, unknown_509d68;  /* zeroed by 0043fbac */
    int32_t unknown_509d78;         /* 00509d78: divided (16.16) by 004c5ce0[004fb7f0 x 8] at the start (0043fbac) */
    int32_t unknown_509d7c;         /* 00509d7c: zeroed by 0043fbac */
    int32_t results_order[9];       /* 00509d88: slots in finishing order (0045af80), 0..8 at the start */
    int32_t unknown_4bc660;         /* 004bc660: zeroed by 0043fbac; 00440ef0 sets it when a player's view moves on */
    uint8_t game_state[0x5b6];      /* 0052ad44: the game / tournament state the SAV file holds; 0x95 bytes a player */
    /* 0051dcbc..0051dccb: the results screen's words (0045b29c) */
    int16_t results_unknown_cbc;
    int16_t results_shown_place;    /* 0051dcbe */
    int16_t results_rival_rank;     /* 0051dcc0 */
    int16_t results_player_rank;    /* 0051dcc2 */
    int16_t results_player_place;   /* 0051dcc4 */
    int16_t results_rival;          /* 0051dcc6 */
    int16_t results_rival_name;     /* 0051dcc8: the rival's short-name index (0042700c) */
    int16_t results_unknown_cca;    /* the 16 words above are one block for the oracle: keep them together */
    int16_t results_order_short[9]; /* 004fa030: results_order as shorts (0045b29c) */
    int16_t unknown_4f82a0[8];      /* 004f82a0: each racer's flags 0045af80 ORs into the game state (+0x5a4) */
    int32_t frontend_track;         /* 00524e85, read as a word: 0..2 stages */
    int32_t frontend_laps_word;     /* 00524e9f: the high half is the laps chosen */
    int32_t force_off;              /* 004fcdd4: non-zero keeps 0043ce08 from sending the force */
    int32_t force_x, force_y;       /* 004bb8f8, 004bb8fc: the force last sent */
    /* 0043ce88's: the x and y shakes on (004bb8ec, 004bb8f0), the level and
     * strength last sent (004bb900, 004bb904) */
    int32_t vibration_x_on, vibration_y_on, vibration_level, vibration_strength;
    int32_t laps_setting;           /* 004fbac4: the laps chosen (0042702c) */
    int32_t record_slot;            /* 004fbad4 (00524ea3): which of a circuit's saved records the rival runs to */
    const uint8_t *records;         /* 00524ec0: the saved track records, 0xa6b bytes a track; NULL reads 0 */
    size_t records_size;
    int32_t finish_cars;            /* 00509d80: cars[0..] whose finish 00440ef0 checks (0043fbac: 1 time trial, 2 head to head, else the racers) */
    int32_t finish_humans;          /* 00509d84: players whose records 00440c30 marks out before the start (0043fbac: 1 in a time trial, else the humans) */
    int32_t manager_countdown;      /* 004f80fc: calls of 0040d8e8 left until the manager runs; 1 from 0040fec8, kept in replays */
    int32_t unknown_4b92d4;         /* 004b92d4: non-zero keeps the manager's car sound off (0046c490); 00472560, 0041f9a0 and others set it */
    int32_t police_node;            /* 004c5d40: the first car's node the police placement last looked from (0046b950) */
    int32_t unknown_4fb7d4;         /* 004fb7d4: 0052b312 != 0 at the start; 1 keeps the police car from being placed (0046b950) */
    /* 00509d6c, 00509d70: the race's end request; 4 once the police have
     * stopped the player twice (0046bb78) */
    int32_t end_request, end_state;
    /* 004c5fa8, 004c5fa4, 004c5fac: a sound held from one car on channel 4
     * (00472e34 plays it from cars[car_sound_slot] while the ticks run down;
     * set by 00472f20, 00472348 and 0047217c). What it is (a horn?) is open. */
    int32_t car_sound_ticks, car_sound_kind, car_sound_slot;
    /* 00504218, 0050421c: the node and the segment the track's scenery was
     * last streamed in for (0043f48c); -1 makes the renderer stream it again */
    int32_t scenery_stream_node, scenery_stream_segment;
    /* 004b9098 (0040d6a0's buffer): two bytes per scenery record, the first
     * the minute (race tick / 0x708) + 1 it was knocked over, 0 standing */
    uint8_t scenery_state[2 * TRI_SCENERY_MAX];
    int32_t unknown_4fbb0c;         /* 004fbb0c: with playback not 1, 004501fc copies each record's first state byte to its second */
    /* 004c5f98, 004c5f9c: read by 0047217c (race_sound_tyres) when the
     * first is not negative; 0044df20 sets both to -1 */
    int32_t unknown_4c5f98, unknown_4c5f9c;
    /* 0052b31e (the German word, re/STATE.md) and 0052b30e (written at
     * 0043234e): the first set and the second 0 keeps 00443820's voices off */
    int16_t unknown_52b31e, unknown_52b30e;
    /* 0044e354's: the minute marks the replay seeks to (005120d8, 0xa2
     * bytes each: minutes 0..15, then the four marks 00512f84 holds the
     * ticks of), what it sets besides (004c5fa0, read by 00472xxx's sounds;
     * 00509e10; 004b9624), 0051372c (1 while a mark's humans come back),
     * 00513728 (1 after a seek to a mark) and 00513734 (the voices start
     * again at the next seek) */
    uint8_t replay_marks[0x14 * 0xa2];
    int32_t replay_mark_ticks[4];
    int32_t unknown_4c5fa0, unknown_509e10, unknown_4b9624;
    int32_t unknown_51372c, unknown_513728, unknown_513734;
    /* 004c5fb0..004c5fbc (00472560): the siren playing (channels 9 / 11),
     * the second siren's count (3 as the exe starts), the police car's
     * node last heard, the siren's pitch (0x40 as the exe starts). In this
     * order: probe_car copies them as one block. */
    int32_t siren_on, siren_count, siren_node, siren_pitch;
    /* 004c5fc0, 004c5fc4: the cars last heard on channels 2 and 3 (00472560) */
    const Car *sound_loud, *sound_second;
    /* 0044e8f8's: the calls left until the next mark (005120d4, 0x1c2
     * after each), the clock 004397f4 takes its slot from (004c5590) and
     * the dropped network game (0052b314) */
    int32_t replay_mark_countdown, input_clock;
    int32_t unknown_512f78;         /* 00512f78: the cut playing; 0044ecc4 hands its word to 0044eb34 when above -2 */
    /* 00512d94: the cuts; cut -1's word is 00512d90, the clock. 0044d7d8
     * writes at most 30; the 30 seconds it picks them from (00512efc) lie
     * where rows 30..39 would (race_replay_cuts.c views them as words). */
    ReplayCut replay_cuts[REPLAY_CUTS];
    int32_t replay_cut_count;       /* 00512f74 */
    int32_t unknown_512f7c;         /* 00512f7c: 0044ecc4 clears it and then marks the tick in 00512f84 */
    int32_t unknown_4c5370;         /* 004c5370: cleared by 0044ecc4 and at 0044e8b3 */
    int32_t unknown_51371c;         /* 0051371c: set, 0044ecc4 steps the camera's kind (0040594c) on ticks & 0xff == 0xfc */
    int16_t dropped;
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
    RaceVoice voices[RACE_VOICES];  /* 0050a4cc */
    int32_t sound_silent;           /* 004bc6ec: non-zero, the followed car's sounds play at volume 0 */
    RaceEngineSound engine_sound;   /* 004bcb6c */
    int32_t unknown_4bc6f0;         /* 004bc6f0: non-zero, the engine's second voice plays (1 as the exe starts) */
    int32_t sound_pitch[RACE_VOICES];  /* 004bc6f4: the pitch a channel keeps while it plays the same sound (00443e70) */
    RaceSounds sounds;
    int32_t port_replay_cameras;    /* port: the extra (extras.cfg): the own live car's view key goes the replay's way */
    int32_t port_follow_camera;     /* port: the experimental extra: every camera follows the car's heading (race_follow.h); 0 the exe's */
    int32_t port_follow_ring;       /* port: with port_follow_camera, the segment cache's ring is TRACK_CACHE_RECORDS_MAX records, not the exe's 64 (the arm behind keeps more); 0 the exe's */
    int32_t port_time_of_day;       /* port: 004fbae0[track], the menus' time of day (0..2) a replay's setup keeps */
    int32_t port_replay_launch;     /* port: 1 the NITRO start is recorded in the replay (the player's N held, as the two gear bits at once,
                                     * race_replay_launch_word) and played back (drive_replay.c), the launch runs in a replay (car_drive.c
                                     * launch) and its clock is the sim's own tick, not start_clock (the timer: a slow replay or a rewind
                                     * re-simulation would count it differently), live and in playback alike; 0 the exe's: a replay never
                                     * launches, the launch goes by start_clock */
    int32_t port_stop_replaced;     /* port: 1 a channel's looping voice (a horn 0x3f..0x41, a skid 0x2a) is cut when another sound takes
                                     * the channel over, 0 the exe's: the voice is left looping, its record lost (race_sound.c) */
    int32_t port_pad_and_keys;      /* port: 1 the followed player's keys and pad are both live (input_read.c), the device words
                                     * of config.dat (0052ad26..) not looked at; 0 the exe's: those words choose one device
                                     * per action */
    int32_t port_free_laps;         /* port: 1 the race's laps are not the exe's (free_laps.h): the places the lap count outgrows
                                     * guard themselves (the splits' 85, the game state's 20 lap times, the finish estimate's
                                     * int32, the AI zones' int16 progress); 0 the exe's, byte for byte */
    int32_t port_reverse;           /* port: 1 the track runs the other way round (tri_reverse.h): no rival times (the splits' tables
                                     * are for the track's own direction); 0 the exe's */
} RaceState;

enum {
    /* 004fb7d8 bit 0x20: falling cars skip the extra half pull (00428bd8) and,
     * except on "tr6", come back from long jumps more slowly */
    TRACK_FLAG_LOW_GRAVITY = 0x20,
    /* 004fb7d8 bit 3, the Turbo cheat: 004275f8 puts the timer on level 3 (0043bb44), the race 9/7 as fast */
    TRACK_FLAG_TURBO = 8
};

/* The car whose record holds `handle` at +0x88 (the original's pointer to
 * the car, see car_reset), or NULL. */
Car *race_car_by_handle(const RaceState *race, uint32_t handle);

/* A slot of the car tables, 0..RACE_CARS - 1; anything else reads slot 0
 * (port guard: the original indexes them unchecked). */
static inline int32_t race_slot(int32_t slot)
{
    return (uint32_t)slot < RACE_CARS ? slot : 0;
}

/* 00440dd8: sorts by_node and by_progress (ties by the lateral +0x39d). */
void race_sort_cars(RaceState *race);

/* 004b9628 * 004b9630: the next random value, 0..0xffff */
uint32_t race_random(RaceState *race);

/* 00428fe0 (004275f8 with 004fb7f4): race_random's state from a seed,
 * the seed times 3 and the multiplier the seed, each made odd. */
void race_random_seed(RaceState *race, uint32_t seed);

/* 00487c2c: the next of the add-with-carry generator: each word added to the
 * next with carry, the lowest counting up, the top word the result. */
uint32_t race_rand(RaceState *race);

/* 00443730: 0..n-1 from the low 16 bits of race_rand (signed product). */
int32_t race_rand_below(RaceState *race, int32_t n);

/* 004571e8, its pick (00455a5c with `clear` 1 at the load's start, 00440858
 * with 0 before slot 2): a picture 0..6 drawn with 00443730(7), drawn again
 * while loading_used has it, then marked; `clear` empties the 20 flags
 * first. Returns the picture (the file "<paths 12><004c5660[the player's
 * car]>s<n>.qfs", shown by the rest of 004571e8: not ported), or -1 with
 * all 7 used, where the original draws for ever. */
int32_t race_loading_pick(RaceState *race, int clear);

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
