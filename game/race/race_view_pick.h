/* The live race's panel: the view picked with the keys (00471a38, from
 * 00471460 while a replay runs or another car is followed). */
#ifndef TNFS_RACE_VIEW_PICK_H
#define TNFS_RACE_VIEW_PICK_H

#include "game/sys/joystick.h"
#include "game/sys/menu_key.h"

#include <stdint.h>

typedef struct {
    int32_t view;       /* 0052b8d4: the view taken */
    int32_t cursor;     /* 0052b8d8: the view the panel points at, 1..12 */
    int32_t redraw;     /* 0052b8f4 */
    int32_t last_key;   /* 004c5f94: the menu key of the frame before */
} RaceViewPick;

typedef struct {
    int32_t racers;     /* 004fb7dc */
    int32_t humans;     /* 004fb7e0 */
    int32_t viewed;     /* 004fbb04 */
    int16_t replay;     /* 0052b316 */
    int32_t present;    /* 004fcdcc */
    int32_t device;     /* 004bb908 */
} RaceViewPickInputs;

/* 00471a38: the menu key (00465358). None: the first of the flags 0x33
 * (view 2), 0x34 (11), 2 (3), 3..9 (4..10, each only with more than 1..7
 * racers, else nothing more) that is down is taken (0052b8d4) with a
 * redraw, 1; none, 0. A key unlike the last one: left moves the cursor
 * down (to 0 from 2 outside a replay), below 1 to 12 when the shown car is
 * a human's (004fbb04 < 004fb7e0) else 11, onto 10 to racers + 2; right
 * up, past 12 (or onto 12 with no human shown) to 1 in a replay else 2,
 * onto 4..10 past racers + 2 to 11; up 2 -> 3, 11 -> racers + 2; down 3 ->
 * 2, racers + 2 -> 11; each move a redraw and 1. Space or return then take
 * the cursor (0052b8d4) with a redraw, 1. The key is kept as the last. */
int32_t race_view_pick(RaceViewPick *p, const uint8_t key_flags[KEY_FLAGS], JoyState *j, const JoyOps *ops,
                       const RaceViewPickInputs *in);

#endif
