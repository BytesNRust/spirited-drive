/* Random picks of the menus. */
#ifndef TNFS_RANDOM_PICK_H
#define TNFS_RANDOM_PICK_H

#include <stdint.h>

/* 00466f34: srand(time(NULL)) and rand() / 0x2aaa, so 0..2, with the top
 * two values of rand (3) as 0. Watcom's rand: state = state x 0x41c64e6d +
 * 0x3039, (state >> 16) & 0x7fff; *rand_state is the C library's state the
 * original leaves behind. */
int random_pick_of_three(uint32_t now, uint32_t *rand_state);

#endif
