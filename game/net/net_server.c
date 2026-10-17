/* The network transport's game server side (net_server.h) */
#include "game/net/net_server.h"

#include "game/net/net_queue.h"

#include <string.h>

const char NET_ADD_NO_SLOT[] = "addnewplayer -- FAILED TO FIND EMPTY PLAYER SLOT, SHOULD HAVE NEVER GOT THIS FAR.";

int32_t net_bit_count(uint32_t bits)
{
    int32_t n = 0;
    for (; bits != 0; bits &= ~(0u - bits)) n++;
    return n;
}

void net_game_info(const NetGameInfoOps *ops, NetServer *v, uint8_t *address, uint32_t id)
{
    NetSession *const s = v->session;
    if (id == 0 && v->word_8 != 0) return;
    int32_t message = NET_GAME_INFO;
    uint8_t out[NET_GAME_INFO_SIZE];
    net_put_big(out, id, 4);
    for (int i = 0; i < 5; i++) {
        const uint8_t *w = v->params + 0x30 + 4 * i;
        net_put_big(out + 0x38 + 4 * i, (uint32_t)w[0] | (uint32_t)w[1] << 8 | (uint32_t)w[2] << 16 | (uint32_t)w[3] << 24, 4);
    }
    net_put_big(out + 4, v->players, 4);
    strncpy((char *)out + 8, (const char *)v->params, 0x20);
    strncpy((char *)out + 0x28, (const char *)v->params + 0x20, 0x10);
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, "GameInfo->", &message, out, NET_GAME_INFO_SIZE);
    ops->driver_send(ops->ctx, s->handle, address, message, out, NET_GAME_INFO_SIZE);
}

static uint32_t net_word(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

uint32_t net_add_player(const NetAddPlayerOps *ops, NetServer *v, const uint8_t *record)
{
    for (uint32_t i = 0; i < net_word(v->params + 0x40) - 1; i++) {
        const uint32_t bit = 1u << (i & 31);
        if ((v->players & bit) != 0) continue;
        v->players |= bit;
        uint8_t *const e = v->entries + i * NET_PLAYER_SIZE;
        memcpy(e, record, NET_PLAYER_SEEN);
        net_put_little(e + NET_PLAYER_SEEN, (uint32_t)*ops->now, 4);
        net_put_little(e + NET_PLAYER_SEEN + 4, i, 4);
        return i;
    }
    ops->message(ops->ctx, NET_ADD_NO_SLOT);
    return 0;
}
