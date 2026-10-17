/* The network transport's services (net_service.h) */
#include "game/net/net_service.h"

#include "game/net/net_queue.h"

#include <stddef.h>

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
