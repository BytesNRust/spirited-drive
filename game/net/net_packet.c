/* The network transport's packet layer (net_packet.h) */
#include "game/net/net_packet.h"

#include <string.h>

const char NET_ACK_TRACE[] = "ACKt";

void net_checksum_put(NetChecksum sum, const uint8_t *data, uint8_t *out, uint32_t length, int32_t n)
{
    if (n <= 0) return;
    memset(out, 0, (size_t)n);
    uint32_t value = sum(data, length);
    for (int32_t i = n - 1; i >= 0; i--, value >>= 8) out[i] = (uint8_t)value;
}

uint8_t net_packet_acked(const NetPacketOps *ops, NetPacketConn *c)
{
    int32_t k = (int32_t)(uint32_t)net_queue_head_key(ops->mutex, &c->unacked);
    if (k == -1) k = c->next_seq;
    k--;
    c->stamp = c->link->now;
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, NET_ACK_TRACE, &k, 0, 0, 0, c->id);
    return (uint8_t)(k & 0x7f);
}

int32_t net_checksum_check(const NetPacketConn *c, const uint8_t *data, uint8_t *out, uint32_t length)
{
    const int32_t n = c->sum_size;
    if (n == 0) return 1;
    if (n < 0 || n > 4) return 0;
    uint8_t kept[4];
    memmove(kept, out, (size_t)n);
    net_checksum_put(c->sum, data, out, length, c->sum_size);
    for (int32_t i = n - 1; i >= 0; i--) {
        if (out[i] == kept[i]) continue;
        memmove(out, kept, (size_t)n);
        return 0;
    }
    return 1;
}

int32_t net_packet_send_control(const NetPacketOps *ops, NetPacket *p, NetPacketConn *c, uint32_t kind)
{
    const uint32_t length = (uint32_t)c->sum_size + 4;
    p->sent = 1;
    if (c->driver->room(c->driver->ctx, c->link) < length) return 0;
    const uint8_t acked = net_packet_acked(ops, c);
    p->wire[0] = 0xfe, p->wire[1] = 0, p->wire[2] = (uint8_t)kind, p->wire[3] = acked;
    net_checksum_put(c->sum, p->wire, p->wire + 4, length, c->sum_size);
    const int32_t sent = c->driver->send(c->driver->ctx, c->link, p->wire, length) == length;
    if (sent && kind <= 0x7f) p->sent = c->link->acked;
    return sent;
}

void net_wait_ticks(const NetWaitOps *ops, const int32_t *clock, int32_t ticks)
{
    const uint32_t end = (uint32_t)ticks + (uint32_t)*clock;
    do ops->service(ops->ctx);
    while ((int32_t)((uint32_t)*clock - end) < 0);
}
