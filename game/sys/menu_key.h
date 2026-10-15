/* The menu key of a frame, from the keyboard's flags or the joystick. */
#ifndef TNFS_MENU_KEY_H
#define TNFS_MENU_KEY_H

#include "game/sys/joystick.h"

#include <stdint.h>

enum { KEY_FLAGS = 0x80 };  /* 004c693c; the key names 00487b84 reads follow at 004c69bc */

/* 00465358 (from 0044f55c and 00471a38; key_flags 004c693c, present
 * 004fcdcc, device 004bb908): the first of space, return, escape, tab,
 * up (0x4800), down (0x5000), left (0x4b00), right (0x4d00) whose key code
 * (00487b84) is down in the flags (00483f8c); else the joystick's keys
 * (0043d4c0): its button key, else its direction key; else 0. */
int32_t menu_key(const uint8_t key_flags[KEY_FLAGS], JoyState *j, int32_t present, int32_t device, const JoyOps *ops);

#endif
