/* The transport's serial driver (net_serial.h). */
#include "game/net/net_serial.h"

#include <stdio.h>

enum {
    CMD_OPEN = 1, CMD_FLUSH_INPUT = 2, CMD_FLUSH_OUTPUT = 3, CMD_CLOSE = 4,
    THREAD_PRIORITY = 2,
    OPEN_EXISTING = 3,
    READ_CONSTANT = 0x19
};

static const uint32_t GENERIC_READ_WRITE = 0xc0000000u, OVERLAPPED_NORMAL = 0x40000080u;

static const char INVALID_PORT[] = "serialopen - INVALID PORT %d\n";  /* 004b2f78 */

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

int32_t net_serial_carrier(NetSerial *s, int32_t port, const NetSerialOps *ops)
{
    int32_t on = 1;
    lock(s, ops);
    if (net_serial_is_open(s, port)) {
        uint32_t status = 0;
        if (ops->modem_status(ops->ctx, s->port[port]->file, &status) != 0) ops->fail(ops->ctx);
        else on = (status & NET_SERIAL_RLSD_ON) != 0;
    }
    unlock(s, ops);
    return on;
}

void net_serial_flush_input(NetSerial *s, int32_t port, const NetSerialOps *ops)
{
    if (net_serial_is_open(s, port)) ops->command(ops->ctx, s, port, CMD_FLUSH_INPUT);
}

void net_serial_flush_output(NetSerial *s, int32_t port, const NetSerialOps *ops)
{
    if (net_serial_is_open(s, port)) ops->command(ops->ctx, s, port, CMD_FLUSH_OUTPUT);
}

void net_serial_close(NetSerial *s, int32_t port, const NetSerialOps *ops)
{
    lock(s, ops);
    if (net_serial_is_open(s, port)) {
        NetSerialPort *const p = s->port[port];
        ops->command(ops->ctx, s, port, CMD_CLOSE);
        unlock(s, ops);
        while (s->port[port] != NULL) ops->sleep(ops->ctx, 1);  /* the thread clears it */
        ops->release(ops->ctx, p);
        s->opened--;
    }
    else unlock(s, ops);
    if (s->opened == 0) {
        ops->thread_kill(ops->ctx, s->thread);
        ops->mutex->close(ops->mutex->ctx, s->wake);
        ops->mutex->close(ops->mutex->ctx, s->lock);
    }
}

void net_serial_open(NetSerial *s, int32_t port, const NetSerialOps *ops)
{
    if (port < 0 || port >= NET_SERIAL_PORTS) {
        ops->fatal(ops->ctx, INVALID_PORT, port);
        return;
    }
    if (s->port[port] != NULL) return;
    if (++s->opened == 1) {
        ops->at_exit(ops->ctx);
        s->wake = ops->create_wake(ops->ctx);
        s->lock = ops->mutex->create(ops->mutex->ctx);
        if (ops->thread_start(ops->ctx, 0, THREAD_PRIORITY, -1, s->thread) == 0) {
            ops->mutex->close(ops->mutex->ctx, s->wake);
            ops->mutex->close(ops->mutex->ctx, s->lock);
            s->opened--;
            return;
        }
    }
    char name[NET_SERIAL_NAME];
    snprintf(name, sizeof name, "\\\\.\\COM%d", port + 1);
    void *const file = ops->create_file(ops->ctx, name, GENERIC_READ_WRITE, 0, OPEN_EXISTING, OVERLAPPED_NORMAL);
    if (file == NET_SERIAL_NO_FILE) {
        ops->fail(ops->ctx);
        return;
    }
    NetSerialPort *const p = ops->alloc(ops->ctx, name, NET_SERIAL_BLOCK, s->pool);
    p->word_08 = 0;
    p->file = file;
    p->in_event = ops->create_event(ops->ctx, 1, 0);
    p->in_0c = p->in_10 = 0;
    p->word_22c = 0;
    p->out_event = ops->create_event(ops->ctx, 1, 0);
    p->out_start = p->out_count = 0;
    const NetSerialTimeouts t = { .read_constant = READ_CONSTANT };
    if (ops->set_timeouts(ops->ctx, file, &t) == 0) ops->fail(ops->ctx);
    s->port[port] = p;
    ops->command(ops->ctx, s, port, CMD_OPEN);
}