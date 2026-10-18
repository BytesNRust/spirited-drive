/* The joystick's options (joyopt.cfg: which buttons and axes are on, where
 * each goes, which are turned over, the force-feedback stick wanted) and the
 * checks on them. */
#ifndef TNFS_JOY_OPTIONS_H
#define TNFS_JOY_OPTIONS_H

#include "game/sys/joystick.h"

/* 0043d1a0 (all registers kept): the defaults; every button and axis on,
 * the force-feedback stick wanted, the buttons on bits and the axes on the
 * words 1, 2, 4, 8 in turn, nothing turned over. */
void joy_options_default(JoyState *j);

/* 0043d37c (all registers kept): what the options leave as read. 004fce14
 * (raw) 0 unless every button is on, none turned over and they land on bits
 * 0..3 in turn; 004fce18 (upright) 0 when an axis on is turned over;
 * 004fce48 (raw_axes) 0 when an axis on goes to a word not its own (1, 2, 4,
 * 8). Each only cleared, never set. */
void joy_options_check(JoyState *j);

#endif
