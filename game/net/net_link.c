/* The transport's packet driver over a session (net_link.h) */
#include "game/net/net_link.h"

#include "game/net/net_server.h"

#include <string.h>

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

int32_t net_link_receive(const NetLinkOps *ops, NetPacketHandle *h, uint8_t *data, int32_t size)
{
    NetRequest local = { 0 };  /* 00477b60's zero bytes */
    local.session = h->session;
    uint8_t address[NET_ADDRESS];
    for (;;) {
        int32_t message = NET_DATA;
        memcpy(address, h->address, NET_ADDRESS);
        const int32_t got = ops->driver_receive(ops->ctx, h->session->handle, address, &message, data, size);
        if (message == 0) break;
        if (h->session->request == NULL) h->session->request = &local;
        if (*ops->trace_level >= 5) ops->serve->trace(ops->serve->ctx, "->DataRecv", &message, data, got);
        net_serve(ops->serve, h->session->request, address, message, data, got);
        if (h->session->request == &local) h->session->request = NULL;
        if (message == NET_DATA) {
            memcpy(h->address, address, NET_ADDRESS);
            return got;
        }
    }
    NetSession *const s = h->session;
    if (s->word_a4 == 0) {
        if (*ops->trace_level >= 5) ops->trace(ops->ctx, "NullFlush ");
        ops->driver_link(ops->ctx, s->handle, NULL, NET_DATA, 0);
    } else if (*ops->trace_level >= 6) {
        ops->trace(ops->ctx, "NullRecv  ");
    }
    return 0;
}

int32_t net_link_send(const NetLinkOps *ops, NetPacketHandle *h, uint8_t *data, int32_t size)
{
    int32_t message = NET_DATA;
    if (*ops->trace_level >= 5) ops->serve->trace(ops->serve->ctx, "DataSend->", &message, data, size);
    return ops->driver_send(ops->ctx, h->session->handle, h->address, message, data, size) != 0 ? size : 0;
}
