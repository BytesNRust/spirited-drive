/* The race's sound levels: how far left or right a sound sits and how loud
 * it is from where it is heard (00443eac and the followed car's engine,
 * 004444b8, use them). */
#ifndef TNFS_RACE_SOUND_H
#define TNFS_RACE_SOUND_H

#include <stdint.h>

/* 00443c48: the pan of a sound at `angle` from the listener's heading
 * (00472018's angle; >> 16 reads 0..0x100 a turn): 0x40 ahead and behind,
 * 0 a quarter turn round, 0x7f three quarters. */
int32_t race_sound_pan(int32_t angle);

#endif
