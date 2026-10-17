#include "game/frontend/random_pick.h"

/* 00466f34 */
int random_pick_of_three(uint32_t now, uint32_t *rand_state)
{
    const uint32_t state = now * 0x41c64e6du + 0x3039u;  /* 00487c1c srand, 00487bf8 rand */
    *rand_state = state;
    const int pick = (int)((state >> 16) & 0x7fff) / 0x2aaa;
    return pick < 0 || pick > 2 ? 0 : pick;
}
