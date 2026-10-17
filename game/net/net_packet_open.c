/* 0047ac18 openpacketconnection (net_packet.h). Under checks that stop the
 * game (00477a40, not returning there): size below 6 is 6; above 0xfa the
 * message 004b2c24 (size kept); else with the driver's +0x1c set and size + 8
 * above it 004b2c64. +0 set: 004b2cb4. Not the main thread (00476968(0)):
 * 004b2cec, else the surface locked (004c7ac0): 004b2d28. A connection there
 * (+0x20): 0047b250 first. bits clamped to 2..7 (unsigned), size raised to
 * the driver's +0x20. The window is 1 << bits, half = window / 2 - 1. The
 * driver's +0x24 bit 0: no checksum; else 0049d158 of 2 bytes (crc32 asked:
 * 004b2d80 too). The receive buffer: sum + 0xfe, doubled without +0x24 bit 2.
 * stride = (size + 0x10 + sum + 3) & ~3; 00474cdc("pktbuffer", 0xc8 +
 * (2 * half + 1) * stride + recv, tag); none: 0.
 * Then handle +0x20 the block, +0x18 00479eb0 (the old kept), +0x14 / +0x1c
 * 00479eb0 when 0, +4 / +8 / +0xc / +0x10 1 / 10 / 25 / 50 when 0. The
 * connection: its 200 bytes from a template on the stack (+0x2c sum, +0x30
 * size, +0x34 window, +0x38 handle +4, +0x40 handle +8, +0xc4 the sum; the
 * rest never set: 0 here), +0 0, +8 the handle, +0xc 004c62f8 (counted up),
 * +4 the driver, +0x14 00476c68, +0x18 / +0x10 / +0x48 0, +0x44 1, queues
 * +0x4c (no key), +0x68, +0x8c, +0xa8 (0047a800, the connection) with +0x84
 * and +0x88 1 before +0x8c's. half packets (+0, +4, +8 0) pushed onto +0x4c
 * (0049c804), half more (+4 = +0x88, counted up) into +0x8c (0049c954), the
 * next the control packet (+0x1c), +0x28 0, +0x24 recv, +0x20 the rest.
 * The driver's +0 (handle) 0: closed down (below). Else 0049c804 onto
 * 0052ce40, +0 = 1000000000, 004c62fc >= 1: 0047f244("OpenPktConn", 0, 0, 0,
 * 0, +0xc); then 00475c14(0), r = handle +0x14 (handle), 00475c14(0), and
 * under the mutex (+0x14 read again for the unlock) +0 = 1000000000 if set;
 * again while +0 set, the connection's +0x10 0 and r 0. +0 0 or r set:
 * +0 = 0, the driver's +4 (handle).
 * +0 set and r 0: +0 = 004c6300, the old +0x18 back when set, 1. Else
 * 004c62fc >= 1: 0047f244("OpenPktConn Failed", ...), 0049c9d8 off 0052ce40,
 * 00475260, +0x20 = 0, 0. */
#include "game/net/net_packet.h"

#include <string.h>

const char NET_OPEN_TOO_BIG[] = "openpacketconnaction -- MAXIMUM SUPPORTED PACKET SIZE IS %d.";
const char NET_OPEN_OVER_DRIVER[] = "openpacketconnection -- PACKETSIZE IS GREATER THAN TRANSPORT LAYER`S MAXIMUM";
const char NET_OPEN_ACTIVE[] = "openpacketconnection - CALLED WITH AN ACTIVE PKTCONN.\n";
const char NET_OPEN_NOT_MAIN[] = "openpacketconnection -- MAY ONLY BE CALLED FROM MAIN THREAD";
const char NET_OPEN_SURFACE[] = "openpacketconnection -- CANNOT OPEN CONNECTION WHILE SURFACE LOCKED (CALL nullwindow)";
const char NET_OPEN_CRC32[] = "openpacketconnection -- crc32 not yet implemented.";
const char NET_OPEN_BUFFER[] = "pktbuffer";
const char NET_OPEN_TRACE[] = "OpenPktConn";
const char NET_OPEN_FAILED[] = "OpenPktConn Failed";

enum { NET_OPENING = 1000000000, NET_CONN_SIZE = 0xc8 };

int32_t net_packet_hook_none(NetPacketHandle *h) { return (void)h, 0; }

static size_t align8(size_t n) { return (n + 7) & ~(size_t)7; }

static size_t native_stride(uint32_t stride) { return align8(offsetof(NetPacket, wire) + (size_t)(stride - 0xc)); }

static uint32_t wire_stride(const NetPacketConn *c) { return (c->max + 0x10 + (uint32_t)c->sum_size + 3) & ~3u; }

size_t net_packet_conn_bytes(int32_t window, uint32_t stride, int32_t recv)
{
    return align8(sizeof(NetPacketConn)) + (size_t)(window - 1) * native_stride(stride) + (size_t)recv;
}

NetPacket *net_packet_at(NetPacketConn *c, int32_t i)
{
    uint8_t *const base = (uint8_t *)c + align8(sizeof(NetPacketConn));
    return (NetPacket *)(void *)(base + (size_t)i * native_stride(wire_stride(c)));
}

/* The checks before anything is built; size clamped */
static uint32_t open_checks(const NetOpenOps *ops, NetPacketHandle *h, const NetDriver *driver, uint32_t size)
{
    if (size < 6) size = 6;
    else if (size > 0xfa) ops->fatal(ops->ctx, NET_OPEN_TOO_BIG, 0xfa);
    else if (driver->max_size != 0 && size + 8 > driver->max_size) ops->fatal(ops->ctx, NET_OPEN_OVER_DRIVER, 0);
    if (h->open != 0) ops->fatal(ops->ctx, NET_OPEN_ACTIVE, 0);
    if (ops->main_thread(ops->ctx, 0) == 0) ops->fatal(ops->ctx, NET_OPEN_NOT_MAIN, 0);
    else if (*ops->surface_locked != 0) ops->fatal(ops->ctx, NET_OPEN_SURFACE, 0);
    if (h->conn != NULL) net_close_packet_connection(ops->close, h);
    return size;
}

static void handle_defaults(NetPacketHandle *h, NetPacketConn *c)
{
    h->conn = c;
    const NetHandleHook poll = h->poll;
    h->hook_18 = net_packet_hook_none;
    if (poll == NULL) h->poll = net_packet_hook_none;
    if (h->hook_1c == NULL) h->hook_1c = net_packet_hook_none;
    if (h->word_4 == 0) h->word_4 = 1;
    if (h->ack_time == 0) h->ack_time = 10;
    if (h->resend_time == 0) h->resend_time = 25;
    if (h->data_time == 0) h->data_time = 50;
}

/* The template's words, then the words set on the block */
static void conn_fill(const NetOpenOps *ops, NetPacketConn *c, NetPacketHandle *h, const NetPacketConn *t)
{
    const NetMutexOps *const mutex = ops->close->packet->mutex;
    *c = *t;
    c->node.next = NULL;
    c->link = h;
    c->id = (uint32_t)*ops->next_id;
    c->driver = t->driver;
    ++*ops->next_id;
    c->mutex = mutex->create(mutex->ctx);
    c->word_18 = 0, c->connected = 0, c->send_seq = 1, c->acked_seq = 0;
    net_queue_init(mutex, &c->spare, NULL, 0);
    net_queue_init(mutex, &c->queue_68, net_packet_seq_key, (uintptr_t)c);
    c->word_84 = 1, c->next_seq = 1;
    net_queue_init(mutex, &c->incoming, net_packet_seq_key, (uintptr_t)c);
    net_queue_init(mutex, &c->queue_a8, net_packet_seq_key, (uintptr_t)c);
}

static void conn_packets(const NetMutexOps *mutex, NetPacketConn *c, int32_t half, int32_t recv)
{
    int32_t i = 0;
    for (int32_t n = half; n-- != 0; i++) {
        NetPacket *const p = net_packet_at(c, i);
        p->node.next = NULL, p->seq = 0, p->sent = 0;
        net_queue_push_front(mutex, &c->spare, &p->node);
    }
    for (int32_t n = half; n-- != 0; i++) {
        NetPacket *const p = net_packet_at(c, i);
        p->node.next = NULL, p->seq = c->next_seq++, p->sent = 0;
        net_queue_insert(mutex, &c->incoming, &p->node);
    }
    c->recv_used = 0;
    c->control = net_packet_at(c, i);
    c->recv_size = recv;
    c->recv = (uint8_t *)net_packet_at(c, i + 1);
}

/* The driver opened: polled until connected, given up or failed; r */
static int32_t open_poll(const NetOpenOps *ops, NetPacketHandle *h, const NetDriver *driver, NetPacketConn *c)
{
    const NetCloseOps *const k = ops->close;
    const NetPacketOps *const p = k->packet;
    net_queue_push_front(p->mutex, k->open, &c->node);
    h->open = NET_OPENING;
    if (*p->trace_level >= 1) p->trace(p->ctx, NET_OPEN_TRACE, NULL, 0, 0, 0, (uint8_t)c->id);
    int32_t r;
    do {
        k->wait->service(k->wait->ctx);
        r = h->poll(h);
        k->wait->service(k->wait->ctx);
        p->mutex->lock(p->mutex->ctx, c->mutex);
        if (h->open != 0) h->open = NET_OPENING;
        p->mutex->unlock(p->mutex->ctx, c->mutex);
    } while (h->open != 0 && c->connected == 0 && r == 0);
    if (h->open == 0 || r != 0) {
        h->open = 0;
        driver->close(driver->ctx, h);
    }
    return r;
}

static int32_t open_finish(const NetOpenOps *ops, NetPacketHandle *h, NetPacketConn *c, int32_t r,
                           NetHandleHook old_18)
{
    if (h->open != 0 && r == 0) {
        h->open = *ops->open_state;
        if (old_18 != NULL) h->hook_18 = old_18;
        return 1;
    }
    const NetCloseOps *const k = ops->close;
    const NetPacketOps *const p = k->packet;
    if (*p->trace_level >= 1) p->trace(p->ctx, NET_OPEN_FAILED, NULL, 0, 0, 0, (uint8_t)c->id);
    net_queue_remove_locked(p->mutex, k->open, &c->node);
    k->release(k->ctx, c);
    h->conn = NULL;
    return 0;
}

int32_t net_open_packet_connection(const NetOpenOps *ops, NetPacketHandle *h, const NetDriver *driver, uint32_t tag,
                                   int32_t crc32, uint32_t size, uint32_t bits)
{
    const NetHandleHook old_18 = h->hook_18;
    size = open_checks(ops, h, driver, size);
    bits = bits < 2 ? 2 : bits > 7 ? 7 : bits;
    if (size < driver->min_size) size = driver->min_size;
    const int32_t window = (int32_t)(1u << bits), half = window / 2 - 1;
    NetPacketConn t = { .max = size, .window = window };
    if ((driver->flags & NET_DRIVER_NO_SUM) == 0) {
        t.sum_size = 2, t.sum = net_crc16;
        if (crc32 != 0) ops->fatal(ops->ctx, NET_OPEN_CRC32, 0);
    }
    int32_t recv = t.sum_size + 0xfe;
    if ((driver->flags & NET_DRIVER_SINGLE) == 0) recv *= 2;
    const uint32_t stride = wire_stride(&t);
    const uint32_t bytes = NET_CONN_SIZE + (uint32_t)(2 * half + 1) * stride + (uint32_t)recv;
    NetPacketConn *const c = ops->alloc(ops->ctx, NET_OPEN_BUFFER, bytes, tag, net_packet_conn_bytes(window, stride, recv));
    if (c == NULL) return 0;
    handle_defaults(h, c);
    t.word_38 = h->word_4, t.stamp = h->ack_time, t.driver = driver;
    conn_fill(ops, c, h, &t);
    conn_packets(ops->close->packet->mutex, c, half, recv);
    const int32_t r = driver->open(driver->ctx, h) != 0 ? open_poll(ops, h, driver, c) : 0;
    return open_finish(ops, h, c, r, old_18);
}

int32_t net_open_net_connection(const NetConnectOps *ops, const NetCheck *c, NetPacketHandle *h, uint32_t tag,
                                int32_t crc32, uint32_t size, uint32_t bits)
{
    net_check(ops->check, c, "opennetconnection");
    if (h->session->tcp != 0) return ops->tcp_open(ops->ctx, h, tag, crc32, size, bits);
    return net_open_packet_connection(ops->open, h, ops->driver, tag, crc32, size, bits);
}
