/* The transport's serial driver (EA's library, "serialopen"): up to eight
 * COM ports, each a 0x450-byte block that a thread (0047ba18) services on
 * the commands 0047b6a8 posts (1 open, 2 flush the input, 3 the output, 4
 * close). The Win32 calls are ops: the port has no serial device. */
#ifndef TNFS_NET_SERIAL_H
#define TNFS_NET_SERIAL_H

#include "game/net/net_queue.h"

#include <stdint.h>

enum {
    NET_SERIAL_PORTS = 8,
    NET_SERIAL_BLOCK = 0x450,  /* 00474c8c's size */
    NET_SERIAL_NAME = 16,      /* 0047be14's stack buffer */
    NET_SERIAL_CLR_DTR = 5,    /* EscapeCommFunction's */
    NET_SERIAL_SET_DTR = 6,
    NET_SERIAL_RLSD_ON = 0x80  /* GetCommModemStatus's carrier */
};

#define NET_SERIAL_NO_FILE ((void *)(intptr_t)-1)  /* INVALID_HANDLE_VALUE */

typedef struct NetSerialPort {   /* 00474c8c's block; the rest is the thread's */
    int32_t command;             /* +0: 0047b6a8's */
    void *file;                  /* +4: CreateFileA's */
    int32_t word_08;
    int32_t in_0c, in_10;        /* +0xc / +0x10: the input's, cleared on command 2 */
    void *in_event;              /* +0x28 */
    int32_t word_22c;
    int32_t out_start, out_count;  /* +0x230 / +0x234: the output ring's (0x200 bytes at +0x250), cleared on command 3 */
    void *out_event;             /* +0x24c */
} NetSerialPort;

typedef struct NetSerial {
    NetSerialPort *port[NET_SERIAL_PORTS];  /* 0052ce60 */
    uint32_t pool;                          /* 004c6304: 00474c8c's */
    int32_t opened;                         /* 004c6308: the ports open */
    void *wake;                             /* 004c630c: the thread's event */
    void *lock;                             /* 004c6310: the ports' mutex */
    void *thread;                           /* 0052ce80: the thread's record */
} NetSerial;

typedef struct NetSerialTimeouts {  /* COMMTIMEOUTS */
    uint32_t read_interval, read_multiplier, read_constant, write_multiplier, write_constant;
} NetSerialTimeouts;

typedef struct NetSerialOps {
    void *ctx;
    const NetMutexOps *mutex;  /* create 00476c68, lock / unlock, close 00476c58 */
    void (*command)(void *ctx, NetSerial *s, int32_t port, int32_t command);  /* 0047b6a8 (cdecl) */
    void (*fail)(void *ctx);                                     /* 0048f9ec: GetLastError kept */
    void (*fatal)(void *ctx, const char *format, int32_t port);  /* 00477a40 (cdecl): does not return */
    int32_t (*escape)(void *ctx, void *file, int32_t function);  /* EscapeCommFunction */
    int32_t (*modem_status)(void *ctx, void *file, uint32_t *status);  /* GetCommModemStatus */
    void (*sleep)(void *ctx, int32_t ms);                        /* 00476914 (cdecl): SleepEx(ms, 1) */
    void (*release)(void *ctx, NetSerialPort *p);                /* 00475260 (cdecl) */
    void (*thread_kill)(void *ctx, void *thread);                /* 004768c4 (cdecl) */
    void (*at_exit)(void *ctx);                                  /* 0049d215(0047b690) */
    void *(*create_wake)(void *ctx);                             /* 00476ba8: CreateEventA(0, 0, 0, 0) */
    /* 00476820 (cdecl (0047ba18, stack, priority, -1, record)): not 0 when started */
    int32_t (*thread_start)(void *ctx, uint32_t stack, int32_t priority, int32_t unused, void *thread);
    void *(*create_file)(void *ctx, const char *name, uint32_t access, uint32_t share, uint32_t creation,
                         uint32_t flags);                        /* CreateFileA(.., 0, .., 0) */
    NetSerialPort *(*alloc)(void *ctx, const char *name, uint32_t size, uint32_t pool);  /* 00474c8c (cdecl) */
    void *(*create_event)(void *ctx, int32_t manual, int32_t initial);  /* CreateEventA(0, .., .., 0) */
    int32_t (*set_timeouts)(void *ctx, void *file, const NetSerialTimeouts *t);  /* SetCommTimeouts */
    int32_t (*room)(void *ctx, NetSerial *s, int32_t port);  /* 0047c454 (cdecl): 0x200 - +0x234 */
    /* 0047c158 (cdecl (port, size, data)): into the output ring */
    int32_t (*queue)(void *ctx, NetSerial *s, int32_t port, int32_t size, const uint8_t *data);
} NetSerialOps;

/* 0047bb1c (cdecl (port)): 1 with the port 0..7 and its block set, else 0. */
int32_t net_serial_is_open(const NetSerial *s, int32_t port);

/* 0047bb48 / 0047bbb8 (cdecl (port)): under 004c6310 (read again for each
 * use), an open port's EscapeCommFunction(+4, 5 / 6); 0 from it: 0048f9ec. */
void net_serial_clear_dtr(NetSerial *s, int32_t port, const NetSerialOps *ops);
void net_serial_set_dtr(NetSerial *s, int32_t port, const NetSerialOps *ops);

/* 0047bc28 (cdecl (port)): under 004c6310, 1 unless the port is open and
 * GetCommModemStatus(+4, &status) returns 0, then (status & 0x80) != 0; it
 * returning not 0: 0048f9ec (the original's test is the wrong way round). */
int32_t net_serial_carrier(NetSerial *s, int32_t port, const NetSerialOps *ops);

/* 0047bcb4 (cdecl (port)): an open port's 0047b6a8(port, 2). */
void net_serial_flush_input(NetSerial *s, int32_t port, const NetSerialOps *ops);

/* 0047bcf0 (cdecl (port)): an open port's 0047b6a8(port, 3). */
void net_serial_flush_output(NetSerial *s, int32_t port, const NetSerialOps *ops);

/* 0047bd2c (cdecl (port)): under 004c6310 an open port's block kept,
 * 0047b6a8(port, 4), unlocked, 00476914(1) until the thread clears its slot,
 * 00475260(block), 004c6308 - 1; not open: unlocked. 004c6308 0 then:
 * 004768c4(0052ce80), 00476c58 on 004c630c and 004c6310. */
void net_serial_close(NetSerial *s, int32_t port, const NetSerialOps *ops);

#endif
