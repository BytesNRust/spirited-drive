/* The network transport's session (net_session.h) */
#include "game/net/net_session.h"

#include <stddef.h>

const char NET_NOT_MAIN_THREAD[] = "%s -- MAY ONLY BE CALLED FROM MAIN THREAD";
const char NET_SURFACE_LOCKED[] = "%s -- CANNOT CALL WHILE SURFACE LOCKED (CALL nullwindow)";
const char NET_KILL_NOT_SERVER[] = "killnetplayer - ONLY THE GAME SERVER MAY CALL killnetplayer.\n";
const char NET_KILL_SERVER[] = "killnetplayer - CANNOT KILL SERVER'S PLAYER.\n";

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

void net_check(const NetCheckOps *ops, const NetCheck *c, const char *name)
{
    if (ops->main_thread(ops->ctx, 0) == 0) ops->fatal(ops->ctx, NET_NOT_MAIN_THREAD, name);
    else if (c->surface_locked != 0) ops->fatal(ops->ctx, NET_SURFACE_LOCKED, name);
    if (c->trace_level >= 1) ops->trace(ops->ctx, name, 0, 0, 0, 0, 0);
}

int32_t net_kill_player(const NetKillOps *ops, const NetCheck *c, NetSession *s, const uint8_t *address,
                        const int32_t *now)
{
    net_check(ops->check, c, "killnetplayer");
    if (s->tcp != 0) return ops->tcp_kill(ops->ctx, s, address);
    if (s->server == NULL) {
        ops->message(ops->ctx, NET_KILL_NOT_SERVER);
        return 0;
    }
    uint8_t *e = net_find_player(ops->equal, s->server, address, now);
    if (e != NULL) {
        const int32_t i = (int32_t)(e - s->server->entries) / NET_PLAYER_SIZE;
        ops->driver_send(ops->ctx, s->handle, e, NET_KILL, 0, 0);
        s->server->players &= ~(1u << (i & 31));
    } else if (net_address_equal(ops->equal, s, address, s->self) != 0) {
        ops->message(ops->ctx, NET_KILL_SERVER);
    }
    return 0;
}
