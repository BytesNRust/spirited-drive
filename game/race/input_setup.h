/* The followed player's controls from config.dat: the devices (004fb724) and
 * the key codes (004fb6dc..004fb700), as the race's start sets them. */
#ifndef TNFS_INPUT_SETUP_H
#define TNFS_INPUT_SETUP_H

#include "game/race/race_state.h"

#include <stdint.h>

enum {
    INPUT_CONFIG_WORDS = 13,  /* 0052ad26: steering, pedals, gears devices, then ten keys */
    INPUT_KEY_HOLD = 8,       /* 004fb6fc: 004381f8's hold key */
    INPUT_KEY_PARTS = 9,      /* 004fb700: the parts key */
    INPUT_KEY_CODES = 10
};

/* 00426ddc (): config words 0, 1, 2 to controls[player]'s steering
 * (0 keys, 4 and up 2, else 1), pedals and gears (0 keys, 4 and up 2, 3 as
 * it is, else 1), signed shorts; words 3..12 each through 00426b28 (with
 * german, 0052b31c) and 00487b84 to codes 0..6, 8, 7, 9 (word 10 is the
 * hold key, word 11 the handbrake); 004fb788 = 0. */
void input_setup(const int16_t *config, int16_t german, int32_t player, ControlSetup *controls, int32_t *codes,
                 uint8_t *w4fb788);

#endif
