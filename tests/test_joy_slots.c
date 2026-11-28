/* SPDX-License-Identifier: GPL-3.0-or-later */
/* joy_slots: which pads stay, which go and which are new when the device list changes. */
#include "game/platform/joy_slots.h"
#include "test.h"

int main(void)
{
    int from[4], gone[4];
    /* a second pad plugged in: the first keeps its slot */
    joy_slots_match((const uint32_t[]){ 7 }, 1, (const uint32_t[]){ 7, 9 }, 2, from, gone);
    CHECK_EQ_INT(from[0], 0);
    CHECK_EQ_INT(from[1], -1);
    CHECK_EQ_INT(gone[0], 0);
    /* the first of two unplugged: the second moves up to slot 0 and keeps its open device, the first is dropped */
    joy_slots_match((const uint32_t[]){ 7, 9 }, 2, (const uint32_t[]){ 9 }, 1, from, gone);
    CHECK_EQ_INT(from[0], 1);
    CHECK_EQ_INT(gone[0], 1);
    CHECK_EQ_INT(gone[1], 0);
    /* the only pad unplugged: nothing is left and it is dropped */
    joy_slots_match((const uint32_t[]){ 7 }, 1, (const uint32_t[]){ 0 }, 0, from, gone);
    CHECK_EQ_INT(gone[0], 1);
    /* plugged in again: a new id, a new slot, nothing carried */
    joy_slots_match((const uint32_t[]){ 0 }, 0, (const uint32_t[]){ 12 }, 1, from, gone);
    CHECK_EQ_INT(from[0], -1);
    /* a pad replaced by another at once: the old is dropped, the new is new */
    joy_slots_match((const uint32_t[]){ 7 }, 1, (const uint32_t[]){ 8 }, 1, from, gone);
    CHECK_EQ_INT(from[0], -1);
    CHECK_EQ_INT(gone[0], 1);
    /* the same lists: all stay in place */
    joy_slots_match((const uint32_t[]){ 3, 4, 5 }, 3, (const uint32_t[]){ 3, 4, 5 }, 3, from, gone);
    CHECK(from[0] == 0 && from[1] == 1 && from[2] == 2 && !gone[0] && !gone[1] && !gone[2]);
    /* a repeated id (never by SDL) is carried once */
    joy_slots_match((const uint32_t[]){ 3 }, 1, (const uint32_t[]){ 3, 3 }, 2, from, gone);
    CHECK(from[0] == 0 && from[1] == -1);
    /* the forces' way: a wheel's constant force is its own; Linux's pad (a sine made of rumble, no constant force) rumbles; a Mac or Windows pad rumbles; a sine alone is used when there is nothing else */
    CHECK_EQ_INT(joy_forces_pick(1, 1, 0), JOY_FORCES_HAPTIC);
    CHECK_EQ_INT(joy_forces_pick(1, 0, 0), JOY_FORCES_HAPTIC);
    CHECK_EQ_INT(joy_forces_pick(0, 1, 1), JOY_FORCES_RUMBLE);
    CHECK_EQ_INT(joy_forces_pick(0, 0, 1), JOY_FORCES_RUMBLE);
    CHECK_EQ_INT(joy_forces_pick(0, 1, 0), JOY_FORCES_HAPTIC);
    CHECK_EQ_INT(joy_forces_pick(0, 0, 0), JOY_FORCES_NONE);
    return TEST_REPORT();
}
