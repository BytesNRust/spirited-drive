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

#endif
