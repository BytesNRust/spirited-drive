/* The transport's serial driver (net_serial.h). */
#include "game/net/net_serial.h"

#include <stdio.h>

enum {
    CMD_OPEN = 1, CMD_FLUSH_INPUT = 2, CMD_FLUSH_OUTPUT = 3, CMD_CLOSE = 4,
    THREAD_PRIORITY = 2,
    OPEN_EXISTING = 3,
    READ_CONSTANT = 0x19
};

int32_t net_serial_is_open(const NetSerial *s, int32_t port)
{
    return port >= 0 && port < NET_SERIAL_PORTS && s->port[port] != NULL;
}

static void lock(const NetSerial *s, const NetSerialOps *ops) { ops->mutex->lock(ops->mutex->ctx, s->lock); }
static void unlock(const NetSerial *s, const NetSerialOps *ops) { ops->mutex->unlock(ops->mutex->ctx, s->lock); }

static void escape(NetSerial *s, int32_t port, int32_t function, const NetSerialOps *ops)
{
    lock(s, ops);
    if (net_serial_is_open(s, port) && ops->escape(ops->ctx, s->port[port]->file, function) == 0) ops->fail(ops->ctx);
    unlock(s, ops);
}

void net_serial_clear_dtr(NetSerial *s, int32_t port, const NetSerialOps *ops)
{
    escape(s, port, NET_SERIAL_CLR_DTR, ops);
}

void net_serial_set_dtr(NetSerial *s, int32_t port, const NetSerialOps *ops)
{
    escape(s, port, NET_SERIAL_SET_DTR, ops);
}