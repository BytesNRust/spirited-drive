/* Race setup chosen in Control Central (00432ff4), single-player rules.
 * The original keeps these in a global block around 00524e5b; field
 * comments give each address. */
#ifndef TNFS_RACE_SETTINGS_H
#define TNFS_RACE_SETTINGS_H

enum {
    RACE_TIME_TRIAL = 0,
    RACE_HEAD_TO_HEAD = 1,
    RACE_SINGLE = 2,
    RACE_TOURNAMENT = 3,
    RACE_TYPES = 4,

    CAR_COUNT = 8,         /* RX-7, NSX, Supra, 911, Corvette, Viper, 512TR, Diablo */
    CAR_WARRIOR = 11,      /* secret car */
    TRACK_COUNT = 8,       /* without Lost Vegas (8) and the hidden track (9) */
    TRACK_LOST_VEGAS = 8,
    TRACK_HIDDEN = 9,

    /* Opponent panel values beyond the cars. */
    OPPONENT_FIELD = 8,       /* single race: the whole field ("slew") */
    OPPONENT_CLOCK = 9,       /* time trial: stopwatch ("watch2") */
    OPPONENT_TOURNAMENT = 10  /* tournament trophy ("tourny1..3") */
};

typedef struct {
    int race_type;   /* 00524e5d */
    int track;       /* 00524e85 */
    int variant_m;   /* 004fbad8: the track's "m" art and layout */
    int variant_r;   /* 004fbadc: the track's "r" art and layout */
    int player_car;  /* 00524e8b */
    int opponent;    /* 00524e81 */
    int laps;        /* 00524ea1 */
    int difficulty;  /* 00524ea3 */
    int car_flag_a;  /* 00524e89 low half, set by 00465ec8 (meaning unknown) */
    int car_flag_b;  /* 00524e7f low half, set by 00465ec8 (meaning unknown) */
    int variant;     /* 00524e83: the race's variant, read by the grid (0042748c) */
} RaceSettings;

/* Content unlocked by tournament progress (from 0052ad3e). */
typedef struct {
    int variant_m;   /* 004baf0c */
    int variant_r;   /* 004baf10 */
    int lost_vegas;  /* 004baf18 */
    int hidden;      /* 004baf14 */
    int warrior;     /* 004c5cc4, a short */
} Unlocks;

/* 00432e48: raises u to the unlock level (the short 0052ad40) and never
 * lowers it: Lost Vegas takes the level itself while it is 0, level >= 1
 * sets the Warrior and the "m" variants, level >= 2 the "r" variants. */
void unlocks_raise(Unlocks *u, int level);

/* 00465ec8: car_flag_a = 0 or 2 and car_flag_b = 1 or 2 from two per-car
 * tables; player_car must be 0..11 (the original reads its own stack past
 * them). */
void race_settings_car_flags(RaceSettings *s);

void race_settings_init(RaceSettings *s);
void race_type_step(RaceSettings *s, const Unlocks *u, int dir);   /* 00432720 + 00432974 */
void player_car_step(RaceSettings *s, const Unlocks *u, int dir);  /* player arrows + 00465ec8 */
void opponent_step(RaceSettings *s, int dir);                      /* opponent arrows */
void track_step(RaceSettings *s, const Unlocks *u, int dir);

/* 00432cd4: the hidden track goes to 0; else track + 1 mod 8 (9 with Lost
 * Vegas), and 0 becomes the hidden track once it is unlocked. */
void race_settings_next_track(RaceSettings *s, const Unlocks *u);

/* 00432c40: base -> "m" -> "r" -> the next track (00432cd4), a variant
 * only when unlocked and never in a tournament. */
void race_settings_track_forward(RaceSettings *s, const Unlocks *u);       /* 00432c40 */

/* 00432d48: the same order backwards. */
void race_settings_track_backward(RaceSettings *s, const Unlocks *u);
void track_changed(RaceSettings *s);                               /* laps, tournament car */

#endif
