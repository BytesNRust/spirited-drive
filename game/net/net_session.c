/* The network transport's session (net_session.h) */
#include "game/net/net_session.h"

#include <stddef.h>

int32_t net_address_equal(const NetEqualOps *ops, NetSession *s, const uint8_t *a, const uint8_t *b)
{
    if (s->tcp != 0) return ops->tcp_equal(ops->ctx, s, a, b);
    return ops->driver_equal(ops->ctx, s->handle, a, b);
}

uint8_t *net_find_player(const NetEqualOps *ops, NetServer *v, const uint8_t *address, const int32_t *now)
{
    for (int i = 0; i < NET_PLAYERS; i++) {
        uint8_t *e = v->entries + NET_PLAYER_SIZE * i;
        if ((v->players >> i & 1) == 0 || net_address_equal(ops, v->session, address, e) == 0) continue;
        for (int k = 0; k < 4; k++) e[NET_PLAYER_SEEN + k] = (uint8_t)((uint32_t)*now >> (8 * k));
        return e;
    }
    return NULL;
}
