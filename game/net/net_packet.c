/* The network transport's packet layer (net_packet.h) */
#include "game/net/net_packet.h"

#include <string.h>

const char NET_ACK_TRACE[] = "ACKt";
const char NET_CLOSE_TRACE[] = "ClosePktConn";
const char NET_CLOSE_NOT_OPEN[] = "closepacketconnection - DID NOT FIND ACTIVE PKTCONN IN CONN LIST.\n";
const char NET_CLOSE_NOT_CLOSED[] = "closepacketconnection - DID NOT FIND PKTCONN IN CLOSED LIST.\n";

uint32_t net_crc16(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xfbea;
    for (int32_t i = 0; i < (int32_t)length; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) crc = (crc & 1) != 0 ? crc >> 1 ^ 0xa001 : crc >> 1;
    }
    return crc;
}

void net_checksum_put(NetChecksum sum, const uint8_t *data, uint8_t *out, uint32_t length, int32_t n)
{
    if (n <= 0) return;
    memset(out, 0, (size_t)n);
    uint32_t value = sum(data, length);
    for (int32_t i = n - 1; i >= 0; i--, value >>= 8) out[i] = (uint8_t)value;
}

uint8_t net_packet_acked(const NetPacketOps *ops, NetPacketConn *c)
{
    int32_t k = (int32_t)(uint32_t)net_queue_head_key(ops->mutex, &c->incoming);
    if (k == -1) k = c->next_seq;
    k--;
    c->stamp = c->link->ack_time;
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, NET_ACK_TRACE, &k, 0, 0, 0, (uint8_t)c->id);
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
    if (sent && kind <= 0x7f) p->sent = c->link->resend_time;
    return sent;
}

void net_wait_ticks(const NetWaitOps *ops, const int32_t *clock, int32_t ticks)
{
    const uint32_t end = (uint32_t)ticks + (uint32_t)*clock;
    do ops->service(ops->ctx);
    while ((int32_t)((uint32_t)*clock - end) < 0);
}

uintptr_t net_packet_seq_key(const NetNode *n, uintptr_t arg)
{
    return (void)arg, (uint32_t)((const NetPacket *)(const void *)n)->seq;
}

static void close_say_goodbye(const NetCloseOps *ops, NetPacketHandle *h, NetPacketConn *c, const NetDriver *driver)
{
    h->open = 0;
    for (int tries = 4; net_packet_send_control(ops->packet, c->control, c, 0xff) == 0 && tries-- != 0;)
        net_wait_ticks(ops->wait, ops->clock, 1);
    net_wait_ticks(ops->wait, ops->clock, 1);
    driver->close(driver->ctx, h);
}

int32_t net_close_packet_connection(const NetCloseOps *ops, NetPacketHandle *h)
{
    if (h == NULL || h->conn == NULL) return 0;
    NetPacketConn *const c = h->conn;
    const NetDriver *const driver = c->driver;
    const NetPacketOps *const p = ops->packet;
    if (*p->trace_level >= 1) p->trace(p->ctx, NET_CLOSE_TRACE, NULL, 0, 0, 0, (uint8_t)c->id);
    if (net_queue_remove_locked(p->mutex, ops->open, &c->node) != 0) {
        if (h->open != 0) close_say_goodbye(ops, h, c, driver);
        net_queue_push_front(p->mutex, ops->closed, &c->node);
    } else if (h->open != 0) {
        ops->message(ops->ctx, NET_CLOSE_NOT_OPEN);
    }
    if (net_queue_remove_locked(p->mutex, ops->closed, &c->node) != 0) {
        net_queue_close(p->mutex, &c->spare);
        net_queue_close(p->mutex, &c->queue_68);
        net_queue_close(p->mutex, &c->incoming);
        net_queue_close(p->mutex, &c->queue_a8);
        p->mutex->close(p->mutex->ctx, c->mutex);
        ops->release(ops->ctx, c);
        h->conn = NULL;
    } else {
        ops->message(ops->ctx, NET_CLOSE_NOT_CLOSED);
    }
    ops->wait->service(ops->wait->ctx);
    return 1;
}

/* A list's head connection's handle (its node first: +8) */
static NetPacketHandle *head_handle(const NetQueue *q) { return ((NetPacketConn *)(void *)q->head)->link; }

void net_packet_shutdown(const NetShutdownOps *ops)
{
    const NetCloseOps *close = ops->close;
    if (*ops->started == 0) return;
    while (net_queue_count(close->open) != 0) net_close_packet_connection(close, head_handle(close->open));
    while (net_queue_count(close->closed) != 0) net_close_packet_connection(close, head_handle(close->closed));
    ops->unservice(ops->ctx);
    net_queue_close(close->packet->mutex, close->open);
    net_queue_close(close->packet->mutex, close->closed);
    close->wait->service(close->wait->ctx);
    *ops->started = 0;
}
