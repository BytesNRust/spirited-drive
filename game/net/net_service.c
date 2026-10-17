/* The network transport's services (net_service.h) */
#include "game/net/net_service.h"

#include "game/net/net_queue.h"

#include <stddef.h>
#include <stdio.h>

int32_t net_kill_service(const NetKillServiceOps *ops, const NetCheck *c, NetSession *s)
{
    net_check(ops->check, c, "killnetservice");
    if (net_queue_place(ops->end->mutex, ops->end->sessions, &s->node) == -1) return 0;
    int32_t r;
    if (s->tcp != 0) {
        r = ops->tcp_kill(ops->ctx, s);
    } else {
        if (s->server != NULL || s->word_9c != 0) net_end_game(ops->end, c, s);
        s->word_a0 = 0;
        while (s->word_a4 != 0) ops->wait->service(ops->wait->ctx);
        ops->driver_close(ops->ctx, s->handle);
        r = ops->driver_stop(ops->ctx);
    }
    net_queue_remove_locked(ops->end->mutex, ops->end->sessions, &s->node);
    if (net_queue_count(ops->end->sessions) == 0) ops->unservice(ops->ctx);
    return r;
}

void net_game_name(const uint8_t *guid, char *out)
{
    const uint32_t d = (uint32_t)guid[0] | (uint32_t)guid[1] << 8 | (uint32_t)guid[2] << 16 | (uint32_t)guid[3] << 24;
    snprintf(out, NET_GAME_NAME, "EAGAME.%08x.%04x%04x.%02x%02x%02x%02x.%02x%02x%02x%02x", d,
             (unsigned)(guid[4] | guid[5] << 8), (unsigned)(guid[6] | guid[7] << 8), guid[8], guid[9], guid[10],
             guid[11], guid[12], guid[13], guid[14], guid[15]);
}
