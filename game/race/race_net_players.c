/* The network menu's list of players (race_net_players.h) */
#include "game/race/race_net_players.h"

#include <stddef.h>
#include <string.h>

const char RACE_NET_PLAYER_LIST[] = "player";

int32_t race_net_player_found(void *ctx, NetSession *s, const uint8_t *record, int32_t self)
{
    RaceNetPlayers *const p = ctx;
    (void)s;
    if (record[RACE_NET_PLAYER_TEXT] == 'V') return 1;
    strcpy((char *)p->list + p->count * RACE_NET_PLAYER_SIZE, (const char *)record + RACE_NET_PLAYER_SELF);
    strcpy((char *)p->list + p->count * RACE_NET_PLAYER_SIZE + RACE_NET_PLAYER_TEXT,
           (const char *)record + RACE_NET_PLAYER_TEXT);
    memcpy(p->list + p->count * RACE_NET_PLAYER_SIZE + RACE_NET_PLAYER_ADDRESS, record, 0x14);
    memcpy(p->list + p->count * RACE_NET_PLAYER_SIZE + RACE_NET_PLAYER_SELF, &self, 4);
    p->count = (int16_t)(p->count + 1);
    return 1;
}

RaceNetPlayers *race_net_players_get(const RaceNetPlayersOps *ops, RaceNetPlayers *p, int16_t *key,
                                     const uint8_t *address)
{
    p->count = 0;
    if (p->list == NULL) p->list = ops->alloc(ops->ctx, RACE_NET_PLAYER_LIST, RACE_NET_PLAYERS_BYTES, 0);
    *key = 0;
    ops->wait_keys_on(ops->ctx);
    const NetPlayerCallback cb = { p, race_net_player_found };
    ops->players(ops->ctx, address, &cb);
    ops->wait_keys_off(ops->ctx);
    return p;
}
