/* The race_net files' shared word helpers (private to game/race/race_net*.c). */
#ifndef TNFS_RACE_NET_WORDS_H
#define TNFS_RACE_NET_WORDS_H

#include "game/race/race_net.h"

#include <stdint.h>

static inline int16_t state_word(const uint8_t *state, int at) { return (int16_t)(uint16_t)(state[at] | state[at + 1] << 8); }
static inline void put_state_word(uint8_t *state, int at, int16_t v) { state[at] = (uint8_t)v, state[at + 1] = (uint8_t)((uint16_t)v >> 8); }


static inline void copy_name(uint8_t *state, int at, const uint8_t *from, int room)
{
    for (int i = 0; i < room && at + i >= 0 && at + i < RACE_NET_STATE; i++)
        if ((state[at + i] = from[i]) == 0) return;
}

static inline uint32_t link_word(const RaceNetSend *s, uint32_t link)
{
    const uint32_t at = link - RACE_NET_PLAYERS_AT, room = (RACE_NET_KIND0_FIRST + RACE_NET_VIEWERS) * RACE_NET_ENTRY_SIZE;
    if (link < RACE_NET_PLAYERS_AT || at > room - 4) return 0;
    const uint8_t *b = (const uint8_t *)s->entries + at;
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

#endif
