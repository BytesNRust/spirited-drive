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
