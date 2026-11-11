/* part 3: the network's start, the serial setup and start, the link up and the stand-ins' block. (split from race_net.h; race_net.h includes all four.) */
#ifndef TNFS_RACE_NET_START_H
#define TNFS_RACE_NET_START_H

#include "game/race/race_net_links.h"
#include "game/race/race_net_session.h"

#include <stdint.h>

/* The network's start (004100ec): its locks, the transport's times and the driver's name */
enum {
    RACE_NET_MUTEX_FAILED = 0x4ad71c,                           /* "Error: All mutexes weren't allocated." */
    RACE_NET_DRIVER_NAME = 0x4ad764, RACE_NET_DRIVER_NAME_G = 0x4ad744,  /* "DOS Compatible IPX Connection" / German */
    RACE_NET_PLAYER_TIME = 0x44c, RACE_NET_PULSE_TIME = 0x32, RACE_NET_OPEN_TIME = 0xbb8, RACE_NET_POOL_TAG = 0x20
};

typedef struct {
    void *ctx;
    uint32_t (*mutex)(void *ctx);                      /* 00476c68: CreateMutexA's handle, 0 failed */
    void (*fatal)(void *ctx, uint32_t text);           /* 00477a40 (cdecl) */
    void (*tcp)(void *ctx);                            /* 0047fab0: the TCP layer's hooks */
    void (*driver)(void *ctx, uint32_t name);          /* 00481e78 (eax the name): the IPX driver listed */
    void (*services)(void *ctx);                       /* 00442514 */
} RaceNetStartupOps;

/* The words 004100ec writes; german read */
typedef struct {
    uint32_t lock;                    /* 004f815c */
    uint32_t chat_lock;               /* 004f824c */
    uint32_t input_lock;              /* 004f8160 */
    int32_t unknown_4c62a4, unknown_4c62a8, unknown_4c6300, unknown_4c6304;  /* the transport's: 004c62a4 now, 004c62a8 stamp, 004c6300 open, 004c6304 pool */
    int32_t unknown_4c636c, unknown_4c637c, unknown_4c6384;
    int16_t german;                   /* 0052b31c */
} RaceNetStartup;

/* 004100ec (watcall (), from 00437540): 004f815c, 004f824c then 004f8160
 * each 00476c68(); any 0 (004f815c and 004f824c read back, the third its
 * eax): 00477a40("Error: All mutexes weren't allocated."), and on. 004c62a4
 * = 0x44c, 004c62a8 = 0x32, 004c6300 = 0xbb8, 004c6304 = 0x20, 004c636c,
 * 004c637c and 004c6384 = 0; 0047fab0(); 00481e78("DOS Compatible IPX
 * Verbindung" with 0052b31c, else "... Connection"); 00442514. */
void race_net_startup(const RaceNetStartupOps *ops, RaceNetStartup *s);

/* 0045147c (watcall (), from 00451320): 0044655c, then entry 0's +4..+0x1c
 * 1, 0x19, 5, 10, 00451450, 004460cc, 00446058; +0x28 0052ac92, +0x2c
 * 1 << (0052ac94 & 31), +0x30 0052ac90, +0x34 / +0x38 0, +0x3c 3, +0x40 1,
 * +0x44 0052ac96 (the words sign-extended), +0x48 0052ac9a, +0x4c 0052acd6,
 * +0x50 0 with 004fbb0c, else 0052acfe. */
void race_net_serial_setup(const RaceNetSerialOps *ops, RaceNetSerial *s);

enum { RACE_NET_SERIAL_HUMANS = 2, RACE_NET_DIRECT = 0, RACE_NET_MODEM = 1, RACE_NET_PLAY_DIRECT = 3, RACE_NET_PLAY_MODEM = 2 };

typedef struct {
    void *ctx;
    void (*setup)(void *ctx);                          /* 0045147c (race_net_serial_setup) */
    int32_t (*connect_direct)(void *ctx, uint32_t entry, int32_t a, int32_t b, int32_t c, int32_t d);  /* 0048fe2c (cdecl) */
    int32_t (*connect_modem)(void *ctx, uint32_t entry, int32_t a, int32_t b, int32_t c, int32_t d);   /* 0048ffdc (cdecl) */
    void (*give_up)(void *ctx);                        /* 00446220 (race_net_give_up) */
} RaceNetSerialStartOps;

/* The words 00451320 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t unknown_52b316;
    int32_t unknown_4fbb0c;
    int16_t host;                     /* 00524e7b */
    uint8_t *state;                   /* 004f8164: RACE_NET_STATE bytes */
    int32_t unknown_513e48;           /* set by 00451450 (Esc) */
    int32_t followed;                 /* 004fbb08 */
    int16_t dropped;                  /* 0052b314 */
    const uint8_t *me;                /* 00524df0: this machine's name (a string) */
    int16_t link_kind;                /* 0052ac98: 0 direct (serial), 1 modem */
    int32_t unknown_4bb8dc;           /* an idle clock (100 a second) */
    int16_t play;                     /* 0052b312 */
} RaceNetSerialStart;

/* 00451320 (watcall (), from 00453000): 0052b316 = 0, 004fbb0c = 00524e7b
 * (sign-extended), 004f81d4 = 0, 004f81d6 = 1 - 00524e7b (16-bit), 00513e48
 * = 0, 004f81d2 = 2, 004fbb08 = 004f81d6, 0052b314 = 0; this machine's name
 * into slot 004f81d6's name; 0045147c; 004bb8dc = 0; 0052ac98 0: 0052b312 =
 * 3 and 0048fe2c(0050a5dc, 0x20, 0, 0xfa, 6), 1: 0052b312 = 2 and 0048ffdc
 * (the same), else 0. 0 back: 00446220, 004fbb08 = 0, 0052b312 = 0, 00524e7b
 * = 1, 004fbb0c = 1. Returns it. Port guard: the name stays in the block. */
int32_t race_net_serial_start(const RaceNetSerialStartOps *ops, RaceNetSerialStart *s);

/* 00451660's texts (English / German) */
enum {
    RACE_NET_TRYING = 0x4b0abc, RACE_NET_TRYING_G = 0x4b0aa0, RACE_NET_LINKED = 0x4b0aec, RACE_NET_LINKED_G = 0x4b0ad4,
    RACE_NET_CUT = 0x4b0b1c, RACE_NET_CUT_G = 0x4b0b04, RACE_NET_DIAL = 0x4b0b4c, RACE_NET_ANSWER = 0x4b0b84,
    RACE_NET_NO_LINK = 0x4b0bb8, RACE_NET_NO_LINK_G = 0x4b0ba4,
    RACE_NET_LINK_TICKS = 200, RACE_NET_WHO_TICKS = 400, RACE_NET_LINK_DEADLINE = 6000,
    RACE_NET_HELLO_KIND = 9, RACE_NET_HELLO = 13  /* kind, 004fbb0c's word, this machine's name */
};

typedef struct {
    void *ctx;
    void (*message)(void *ctx, uint32_t text, int32_t ticks);           /* 00467214, the frontend's menu library */
    void (*message_text)(void *ctx, const uint8_t *text, int32_t ticks); /* the same with 00473d43's text */
    void (*menu_a)(void *ctx);                                          /* 004644f0, the frontend's */
    void (*menu_b)(void *ctx);                                          /* 00464568, the frontend's */
    int32_t (*start)(void *ctx);                                        /* 00451320 (race_net_serial_start) */
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
    void (*deadline)(void *ctx, int32_t ticks);                         /* 00410b7c */
    int32_t (*step)(void *ctx);                                         /* 00410bac (race_net_wait_step) */
    void (*give_up)(void *ctx);                                         /* 00446220 (race_net_give_up) */
    void (*seats)(void *ctx);                                           /* 004465e4 */
    void (*sync)(void *ctx);                                            /* 004104d0 (race_net_sync) */
    void (*after)(void *ctx);                                           /* 004562e8 */
} RaceNetLinkOps;

/* The words 00451660 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int32_t unknown_513e44;           /* set by the link's handlers: nobody dialled / answered */
    int32_t unknown_513e48;           /* set by 00451450 (Esc) */
    int16_t german;                   /* 0052b31c */
    int16_t unknown_52b304;           /* 0 while it runs, then back */
    int16_t play;                     /* 0052b312 */
    int16_t dropped;                  /* 0052b314 */
    int32_t joined;                   /* 0050a5a8 */
    int32_t unknown_4fbb0c;
    int16_t unknown_50a5ba, unknown_50a5be;
    const uint8_t *me;                /* 00524df0: this machine's name (a string) */
} RaceNetLinkUp;

/* 00451660 (watcall (), from 00453000): the link connect action. 00513e44
 * = 0; "Attempting to connect" (-1 ticks); 0052b304 kept; 004644f0,
 * 00464568; 0052b304 = 0, 0050a5a8 = 0; 00451320 answering: the 13-byte
 * hello {9, 004fbb0c's word, the name} to 0050a5dc, 00410b7c(6000), until
 * 0050a5be 00410bac (answering: 00446220, the wait ends); the host
 * (004fbb0c) then 004465e4 and 0050a5ba = 1, a client 00410b7c(6000) and
 * the same wait on 0050a5ba. Then 0052b314 set: 0052b312 = 0, else with
 * 0052b312 004104d0. The result for 200 ticks: 0052b312 "Successfully
 * connected" (0050a5a8 + 1), 00513e48 "Connection Aborted", else (after,
 * with 00513e44, "One person needs to Dial / Answer" for 400) "Connection
 * Failed"; the German ones, the who-dials texts formatted with 0xe1 / 0x84.
 * 0052b304 back, 004562e8, 0052b314 = 0 without 0052b312; returns 0052b312. */
int32_t race_net_link_up(const RaceNetLinkOps *ops, RaceNetLinkUp *l);

#endif
