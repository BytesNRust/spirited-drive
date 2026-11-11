/* port (wrong_way): the "WRONG WAY" warning's judgement. The car driving forwards (not in reverse, its own-frame forward speed above
 * WRONG_WAY_MIN_SPEED) with its nose more than a quarter turn from the road's heading at its node, not spinning, for WRONG_WAY_ON_MS, and the
 * warning is on; WRONG_WAY_OFF_MS of anything else and it is off. Reversing is manoeuvring: it never turns the warning on, whichever way the
 * nose points (the owner, 2026-11-11). Pure: no race, no drawing (viewer/wrong_way_draw.c draws it). Angles are the race's 24-bit ones
 * (0x1000000 a turn), speeds 16.16 m/s, the yaw rate the car's (24-bit a second). */
#ifndef TNFS_WRONG_WAY_H
#define TNFS_WRONG_WAY_H

#include <stdint.h>

enum {
    WRONG_WAY_MIN_SPEED = 8 << 16,      /* 8 m/s (about 30 km/h) forwards: slower is a manoeuvre */
    WRONG_WAY_SPIN_RATE = 0x200000,     /* half a turn a second: faster is a spin */
    WRONG_WAY_ON_MS = 1500,             /* wrong this long before the warning */
    WRONG_WAY_OFF_MS = 500,             /* and right this long before it goes */
    WRONG_WAY_BLINK_MS = 250            /* lit 250 ms, dark 250 ms: 2 Hz */
};

typedef struct {
    int on;          /* the warning is up */
    int wrong_ms;    /* how long the car has been going the wrong way (while off) */
    int right_ms;    /* how long it has not (while on) */
    int shown_ms;    /* how long the warning has been up, for the blink */
} WrongWay;

/* the car's state a step, as the judgement reads it */
typedef struct {
    int running;              /* 0 in the countdown, after the car's finish, in a replay: the warning goes at once */
    int32_t road_yaw;         /* the road's heading at the car's node */
    int32_t nose_yaw;         /* the car's yaw */
    int32_t forward_speed;    /* the car's own-frame forward speed (local_velocity_z), negative backwards */
    int reverse_gear;         /* in reverse */
    int32_t yaw_rate;
} WrongWayInput;

/* one step of dt_ms. Returns w->on. */
int wrong_way_step(WrongWay *w, const WrongWayInput *in, int dt_ms);

/* the warning is drawn this step: on, and in the lit half of the blink */
int wrong_way_visible(const WrongWay *w);

/* the words: "WRONG WAY", "FALSCHE RICHTUNG" in German */
const char *wrong_way_text(int german);

#endif
