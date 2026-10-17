/* The network transport's packet layer (net_packet.h) */
#include "game/net/net_packet.h"

#include <string.h>

const char NET_ACK_TRACE[] = "ACKt";
const char NET_RESEND_TRACE[] = "rsnd";
const char NET_SENT_TRACE[] = "sent";
const char NET_WRITE_FAIL_TRACE[] = "write failure";
const char NET_BUSY_TRACE[] = "busy";
const char NET_HOLD_TRACE[] = "hold";
const char NET_NAK_TRACE[] = "NAKt";
const char NET_ACK_NEW_TRACE[] = "ackr";
const char NET_ACK_OLD_TRACE[] = "ack?";
const char NET_NAK_GOT_TRACE[] = "nakr";
const char NET_CLOSE_TRACE[] = "ClosePktConn";
const char NET_CLOSE_NOT_OPEN[] = "closepacketconnection - DID NOT FIND ACTIVE PKTCONN IN CONN LIST.\n";
const char NET_CLOSE_NOT_CLOSED[] = "closepacketconnection - DID NOT FIND PKTCONN IN CLOSED LIST.\n";
const char NET_SHUTDOWN_TRACE[] = "shutdownconn";
const char NET_DISCONNECT_TRACE[] = "disconnect";
const char NET_CONN_CLOSED_TRACE[] = "connclosed";
const char NET_DUP_SHUTDOWN_TRACE[] = "dup shutdown";
const char NET_QUEUED_TRACE[] = "qdat";
const char NET_CLOSE_MSG_TRACE[] = "closeconnmsg";

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

void net_packet_fill(const NetMutexOps *mutex, NetPacket *p, NetPacketConn *c, const uint8_t *data, uint32_t n)
{
    uint8_t *const out = p->wire + 4 + c->sum_size;
    mutex->lock(mutex->ctx, c->mutex);
    p->seq = c->send_seq;
    c->send_seq = (int32_t)((uint32_t)p->seq + 1);  /* wraps as the original's */
    mutex->unlock(mutex->ctx, c->mutex);
    p->sent = 0;
    p->wire[0] = 0xfe, p->wire[1] = (uint8_t)n, p->wire[2] = (uint8_t)(p->seq & 0x7f);
    memmove(out, data, n);
}

int32_t net_packet_send_data(const NetPacketOps *ops, NetPacket *p, NetPacketConn *c)
{
    int32_t length = c->sum_size + p->wire[1] + 4;
    NetPacketHandle *const h = c->link;
    int32_t sent = 0;
    if (c->driver->room(c->driver->ctx, h) < (uint32_t)length) {
        const int32_t place = net_queue_place(ops->mutex, &c->queue_68, &p->node);
        if (*ops->trace_level >= 3)
            ops->trace(ops->ctx, place == -1 ? NET_BUSY_TRACE : NET_HOLD_TRACE, &p->seq, NULL, 0, 1, (uint8_t)c->id);
    } else {
        const uint8_t acked = net_packet_acked(ops, c);
        length = c->sum_size + p->wire[1] + 4;
        p->wire[3] = acked;
        net_checksum_put(c->sum, p->wire, p->wire + 4, (uint32_t)length, c->sum_size);
        if (c->driver->send(c->driver->ctx, h, p->wire, (uint32_t)length) == (uint32_t)length) {
            if (*ops->trace_level >= 3)
                ops->trace(ops->ctx, p->sent != 0 ? NET_RESEND_TRACE : NET_SENT_TRACE, &p->seq,
                           p->wire + 4 + c->sum_size, p->wire[1], 0, (uint8_t)c->id);
            sent = 1;
        } else if (*ops->trace_level >= 1) {
            ops->trace(ops->ctx, NET_WRITE_FAIL_TRACE, &length, p->wire, length, 1, (uint8_t)c->id);
        }
    }
    p->sent = sent ? h->data_time : 1;
    return sent;
}

int32_t net_packet_nak(const NetPacketOps *ops, NetPacket *p, NetPacketConn *c)
{
    if (p == NULL) return 1;
    const int32_t left = p->sent;
    p->sent = (int32_t)((uint32_t)left - 1);
    if (left > 0) return 1;
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, NET_NAK_TRACE, &p->seq, NULL, 0, 1, (uint8_t)c->id);
    return net_packet_send_control(ops, c->control, c, (uint32_t)p->seq & 0x7f);
}

void net_packet_ack(const NetPacketOps *ops, NetPacketConn *c, uint8_t ack)
{
    const uint32_t acked = (uint32_t)c->acked_seq;
    uint32_t k = (acked & ~0x7fu) | ack;
    if (k < acked) k += 0x80;
    int32_t seq = (int32_t)k;
    const char *name = NET_ACK_OLD_TRACE;
    if (k < (uint32_t)c->send_seq && k > (uint32_t)c->acked_seq) c->acked_seq = seq, name = NET_ACK_NEW_TRACE;
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, name, &seq, NULL, 0, 0, (uint8_t)c->id);
    while ((uint32_t)net_queue_head_key(ops->mutex, &c->queue_68) <= (uint32_t)c->acked_seq) {
        NetNode *const n = net_queue_pop_front(ops->mutex, &c->queue_68);
        if (n == NULL) break;
        net_queue_push_front(ops->mutex, &c->spare, n);
    }
}

void net_packet_nak_got(const NetPacketOps *ops, NetPacketConn *c, uint8_t nak)
{
    const uint32_t head = (uint32_t)net_queue_head_key(ops->mutex, &c->queue_68);
    uint32_t k = (head & ~0x7fu) | nak;
    if (k < head) k += 0x80;
    int32_t seq = (int32_t)k;
    NetPacket *const p = (NetPacket *)net_queue_find_key(ops->mutex, &c->queue_68, k);
    if (p == NULL) return;
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, NET_NAK_GOT_TRACE, &seq, NULL, 0, 1, (uint8_t)c->id);
    p->sent = 0;
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

void net_packet_conn_shutdown(const NetCloseOps *ops, NetPacketConn *c)
{
    const NetPacketOps *const p = ops->packet;
    if (*p->trace_level >= 2) p->trace(p->ctx, NET_SHUTDOWN_TRACE, NULL, NULL, 0, 1, (uint8_t)c->id);
    if (net_queue_remove_locked(p->mutex, ops->open, &c->node) == 0) {
        if (*p->trace_level >= 1) p->trace(p->ctx, NET_DUP_SHUTDOWN_TRACE, NULL, NULL, 0, 1, (uint8_t)c->id);
        return;
    }
    if (*p->trace_level >= 1) p->trace(p->ctx, NET_DISCONNECT_TRACE, NULL, NULL, 0, 1, (uint8_t)c->id);
    c->link->open = 0;
    c->driver->close(c->driver->ctx, c->link);
    c->link->hook_18(c->link);
    net_queue_push_front(p->mutex, ops->closed, &c->node);
    if (*p->trace_level >= 2) p->trace(p->ctx, NET_CONN_CLOSED_TRACE, NULL, NULL, 0, 1, (uint8_t)c->id);
}

/* 0047a50c's data packet: into its receive slot, onto +0xa8 */
static void receive_data(const NetPacketRecvOps *ops, NetPacketConn *c, int32_t *queued, const uint8_t *header,
                         uint32_t size)
{
    const NetPacketOps *const p = ops->close->packet;
    const uint32_t base = (uint32_t)c->word_84;
    uint32_t k = (base & ~0x7fu) | header[2];
    if (k < base) k += 0x80;
    NetPacket *const slot = (NetPacket *)net_queue_fetch(p->mutex, ops->message, &c->incoming, k);
    if (slot == NULL) return;
    memmove(slot->wire, header, size);
    net_queue_insert(p->mutex, &c->queue_a8, &slot->node);
    *queued = (int32_t)((uint32_t)*queued + 1);
    if (*p->trace_level >= 3)
        p->trace(p->ctx, NET_QUEUED_TRACE, &slot->seq, slot->wire + 4 + c->sum_size, slot->wire[1], 0, (uint8_t)c->id);
}

/* 0047a50c's checked packet: size bytes at header */
static void receive_packet(const NetPacketRecvOps *ops, NetPacketConn *c, int32_t *queued, const uint8_t *header,
                           uint32_t size)
{
    const NetPacketOps *const p = ops->close->packet;
    NetPacketHandle *const h = c->link;
    if (h->open != 0) h->open = *ops->open_state;
    c->connected = (int32_t)((uint32_t)c->connected + 1);
    const uint8_t ack = header[3];
    if (ack <= 0x7f) net_packet_ack(p, c, ack);
    if (header[1] != 0 && header[2] <= 0x7f) {
        receive_data(ops, c, queued, header, size);
    } else if (header[2] <= 0x7f) {
        net_packet_nak_got(p, c, header[2]);
    } else if (header[2] == 0xff) {
        if (*p->trace_level >= 2) p->trace(p->ctx, NET_CLOSE_MSG_TRACE, NULL, NULL, 0, 1, (uint8_t)c->id);
        net_packet_conn_shutdown(ops->close, c);
    }
}

uint32_t net_packet_receive(const NetPacketRecvOps *ops, NetPacketConn *c, int32_t *queued, uint8_t *data,
                            uint32_t length)
{
    uint32_t at = 0, used = 0;
    if ((c->driver->flags & NET_DRIVER_SINGLE) == 0)
        while (at < length && data[at] != 0xfe) at++;
    uint8_t *const header = data + at;
    if (*header != 0xfe) return length;
    const uint32_t end = at + 4 + (uint32_t)c->sum_size;
    if (end <= length) {
        const uint32_t n = header[1];
        if (n > c->max) {
            at += n != 0xfe ? 2 : 1;
        } else if (end + n <= length) {
            const uint32_t size = end + n - at;
            if (net_checksum_check(c, header, header + 4, size) == 0) {
                at += 3;
            } else {
                at += size;
                used = size;
                receive_packet(ops, c, queued, header, size);
            }
        }
    }
    c->word_3c = (int32_t)((uint32_t)c->word_3c + (at > used));
    return at;
}

int32_t net_packet_poll(const NetPacketRecvOps *ops, NetPacketConn *c)
{
    int32_t queued = 0;
    NetPacketHandle *const h = c->link;
    const NetDriver *const d = c->driver;
    c->word_38 = (int32_t)((uint32_t)c->word_38 - 1);
    if (c->word_38 != 0) return 0;
    c->word_38 = h->word_4;
    if (d->flags & NET_DRIVER_SINGLE) c->recv_used = 0;
    if ((uint32_t)c->recv_used < (uint32_t)c->recv_size && d->ready(d->ctx, h) != 0) {
        const uint32_t used = (uint32_t)c->recv_used;
        const uint32_t got = d->read(d->ctx, h, c->recv + used, (uint32_t)c->recv_size - used);
        c->recv_used = (int32_t)((uint32_t)c->recv_used + got);
    }
    uint32_t used = 0, left = (uint32_t)c->recv_used, r;
    do {
        r = net_packet_receive(ops, c, &queued, c->recv + used, left);
        if (d->flags & NET_DRIVER_SINGLE) {
            left = d->ready(d->ctx, h) != 0 ? d->read(d->ctx, h, c->recv, (uint32_t)c->recv_size) : 0;
        } else if (r != 0) {
            used += r, left -= r;
        }
    } while (r != 0 && h->open != 0);
    if ((d->flags & NET_DRIVER_SINGLE) == 0 && used != 0) {
        memmove(c->recv, c->recv + used, left);
        c->recv_used = (int32_t)left;
    }
    return queued != 0;
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

void net_packet_start(const NetStartOps *ops)
{
    if (*ops->started != 0) return;
    net_queue_init(ops->mutex, ops->open, NULL, 0);
    net_queue_init(ops->mutex, ops->closed, NULL, 0);
    ops->service(ops->ctx);
    ops->at_exit(ops->ctx);
    *ops->started = 1;
}

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
