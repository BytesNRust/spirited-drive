/* The transport's packet driver over a session (net_link.h) */
#include "game/net/net_link.h"

#include "game/net/net_server.h"

int32_t net_link_open(const NetLinkOps *ops, NetPacketHandle *h)
{
    NetSession *const s = h->session;
    if (s->handle == 0) {
        h->linked = 0;
        return h->linked;
    }
    if (ops->driver_prepare != NULL) ops->driver_prepare(ops->ctx, s->handle, h->address);
    ops->driver_link(ops->ctx, s->handle, h->address, NET_DATA, 0);
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, "Flushed1");
    ops->mutex->lock(ops->mutex->ctx, *ops->lock);
    s->word_a4++;
    ops->mutex->unlock(ops->mutex->ctx, *ops->lock);
    h->linked = 1;
    return h->linked;
}

int32_t net_link_close(const NetLinkOps *ops, NetPacketHandle *h)
{
    NetSession *const s = h->session;
    if (s->handle != 0 && s->word_a4 != 0) {
        ops->mutex->lock(ops->mutex->ctx, *ops->lock);
        s->word_a4--;
        ops->mutex->unlock(ops->mutex->ctx, *ops->lock);
        ops->driver_link(ops->ctx, s->handle, h->address, NET_DATA, 1);
        if (*ops->trace_level >= 3) ops->trace(ops->ctx, "Flushed2");
    }
    h->linked = 0;
    return 1;
}

int32_t net_link_alive(const NetPacketHandle *h)
{
    return h->linked != 0 && h->session->word_a0 != 0;
}

uint32_t net_link_room(const NetLinkOps *ops, const NetPacketHandle *h)
{
    return ops->driver_ready(ops->ctx, h->session->handle) != 0 ? *ops->room : 0;
}

int32_t net_link_query(const NetLinkOps *ops, const NetPacketHandle *h)
{
    return ops->driver_query(ops->ctx, h->session->handle);
}
