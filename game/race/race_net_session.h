/* part 2: the host's start, the connect, the resets and releases, the leave and the give-up, the syncs and the waits, the readiness words. (split from race_net.h; race_net.h includes all four.) */
#ifndef TNFS_RACE_NET_SESSION_H
#define TNFS_RACE_NET_SESSION_H

#include "game/race/race_net_links.h"

#include <stdint.h>

/* The host's start: the session's name (0050a320, 0x40 bytes before 0050a360), the joins' handler 00442ec0 the
 * transport calls, the four messages (00467214's text) and how long they stay */
enum {
    RACE_NET_SESSION_NAME = 0x40, RACE_NET_SESSION_KIND = 0xc, RACE_NET_JOIN_HANDLER = 0x442ec0,
    RACE_NET_CREATED = 0x4aff00, RACE_NET_CREATED_G = 0x4afef0, RACE_NET_CREATE_FAILED = 0x4afedc,
    RACE_NET_CREATE_FAILED_G = 0x4afeb8, RACE_NET_CREATED_TICKS = 200, RACE_NET_FAILED_TICKS = 300
};

typedef struct {
    void *ctx;
    void (*reset)(void *ctx);                                       /* 0044655c */
    int32_t (*create)(void *ctx, uint32_t session, const uint8_t *name, const uint8_t *password,
                      uint32_t handler);                            /* 00479338 (cdecl), the transport's: 0 failed */
    void (*message)(void *ctx, uint32_t text, int32_t ticks);       /* 00467214, the menu library's */
    void (*shut_down)(void *ctx);                                   /* 0047ab80, the transport's */
    void (*release)(void *ctx);                                     /* 00442dec (race_net_release) */
} RaceNetHostOps;

/* The words 00442970 writes and reads; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t host;                     /* 00524e7b */
    int16_t dropped;                  /* 0052b314 */
    int16_t play;                     /* 0052b312 */
    int16_t german;                   /* 0052b31c */
    int16_t unknown_4bc6ac;
    int32_t unknown_4fbb0c;
    int32_t joined;                   /* 0050a5a8 */
    RaceNetCounts counts;             /* 004bc6d8 / 004bc6da */
    int32_t queue_head, unknown_50a36c, queue_count;  /* 0050a368, 0050a36c, 0050a370 */
    const uint8_t *me;                /* 00524df0: this machine's name (a string) */
    uint8_t *state;                   /* 004f8164: RACE_NET_STATE bytes */
    uint8_t *session_name;            /* 0050a320: RACE_NET_SESSION_NAME bytes */
    int32_t unknown_50a360;
} RaceNetHost;

/* 00442970 (watcall (name), from 00453ef4): 00524e7b = 1, 0052b314 = 0,
 * 004f81d6 = 0, 004f81d2 = 1, 004bc6d8 = 1, 004bc6da = 0, 004fbb0c = 1,
 * 0050a5a8 = 0, 004f81d4 = 0, the queue's 0050a368 / 0050a36c / 0050a370 = 0;
 * this machine's name to 004f8166, `name` to 0050a320, 0050a360 = 0xc;
 * 0044655c; 00479338(0050a260, 0050a320, "", 00442ec0): 0 shows "Create Game
 * Failed" (0052b31c: the German one) for 300 and 0047ab80, returns 0; else
 * 004bc6ac = 1, 0052b312 = 1, 00442dec, "Game Created" for 200, returns 1.
 * Strings copied up to their NUL (port guard: within the destination). */
int32_t race_net_host_start(const RaceNetHostOps *ops, RaceNetHost *h, const uint8_t *name);

/* The connection's messages (00467214's text; the German ones when 0052b31c) and how long they stay (-1 until the
 * next one) */
enum {
    RACE_NET_CONNECT_FAILED = 0x4aff2c, RACE_NET_CONNECT_FAILED_G = 0x4aff10,
    RACE_NET_CONNECTING = 0x4aff58, RACE_NET_CONNECTING_G = 0x4aff40,
    RACE_NET_ABORTED = 0x4aff88, RACE_NET_ABORTED_G = 0x4aff70,
    RACE_NET_LINK_FAILED = 0x4affb0, RACE_NET_LINK_FAILED_G = 0x4aff9c,
    RACE_NET_CONNECTED = 0x4affdc, RACE_NET_CONNECTED_G = 0x4affc4,
    RACE_NET_MESSAGE_STAYS = -1, RACE_NET_ABORTED_TICKS = 200
};

typedef struct {
    void *ctx;
    int32_t (*connect)(void *ctx, uint32_t session, const uint8_t *address,
                       const uint8_t *password);                 /* 00479250 (cdecl), the transport's: above 0 connected */
    void (*message)(void *ctx, uint32_t text, int32_t ticks);    /* 00467214, the menu library's */
    void (*service)(void *ctx);                                  /* 00475c14(0) */
    void (*reset)(void *ctx);                                    /* 0044655c */
    int32_t (*join)(void *ctx, const RaceNetLink *entry);        /* 00479a6c(entry, 0x20, 0, 0xfa, 6), cdecl */
    void (*shut_down)(void *ctx);                                /* 0047ab80, the transport's */
    void (*deadline)(void *ctx, int32_t ticks);                  /* 00410b7c: 004f8158 = clock + ticks, -1 none */
    int32_t (*wait)(void *ctx);                                  /* 00410bac (race_net_wait_step) */
    void (*give_up)(void *ctx);                                  /* 00446220 */
    void (*lock)(void *ctx, uint32_t lock);                      /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);                    /* 00476cfc (cdecl) */
    void (*release)(void *ctx);                                  /* 00442dec */
} RaceNetConnectOps;

/* The words 00442ad0 writes and reads; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t host;                     /* 00524e7b */
    int16_t dropped;                  /* 0052b314 */
    int16_t play;                     /* 0052b312 */
    int16_t german;                   /* 0052b31c */
    int16_t unknown_52b316;
    int16_t unknown_4bc6dc;           /* set 0 before the join; the transport sets it when the link was cut */
    int16_t unknown_50a5bc;           /* set when the host's state came in */
    uint8_t esc;                      /* 004c693d: Esc held */
    int32_t unknown_4fbb0c;
    int32_t followed;                 /* 004fbb08 */
    int32_t connected;                /* 0050a2f8: the transport's, set once the session answers */
    uint32_t lock;                    /* 004f815c */
    uint8_t *state;                   /* 004f8164: RACE_NET_STATE bytes */
    const uint8_t *incoming;          /* 004f81d8: the host's state block, RACE_NET_STATE bytes */
    const uint8_t *link_name;         /* 0050a274: RACE_NET_NAME bytes */
    RaceNetLink *entries;             /* 0050a5dc: RACE_NET_ENTRIES */
} RaceNetConnect;

/* 00442ad0 (watcall (address), from 00453ef4): 00524e7b = 0, 004fbb0c = 0,
 * 0052b314 = 0; 00479250(0050a260, address, "") not above 0: "Failed To
 * Connect" for 300, 0. Else "Connecting to server..." until the next; while
 * 0050a2f8 is 0: Esc (004c693d) held waits with 00475c14(0) until let go and
 * ends with "Connection Aborted" for 300, 0; else 00475c14(0). Then 0044655c,
 * 004bc6dc = 0, entry 0 filled (00442e3c(0050a260, 0050a274, 0, 1)) and
 * joined (00479a6c): 0 shows "Connection Aborted" for 200 when 004bc6dc was
 * set, else "Connection Failed" for 300, 0047ab80, 0. Else 00410b7c(-1); while
 * 0050a5bc is 0: 00410bac, nonzero runs 00446220 and ends "Connection
 * Aborted" for 300, 0. Then inside the lock 004f815c (read anew for the
 * unlock) the host's block 004f81d8 copied over 004f8164 (0x74 bytes),
 * 0050a5bc = 0, 004fbb08 = 004f81d6 (this machine's slot, sign-extended);
 * 0052b312 = 1, 0052b316 = (004f81d6 == 0), 00442dec, "Successfully
 * connected" for 200, 1. */
int32_t race_net_connect(const RaceNetConnectOps *ops, RaceNetConnect *c, const uint8_t *address);

/* Two word tables after 0050a5a8: eleven at 0050a5ac (0050a5bc, set when the host's state came in, is [8]) and
 * twelve at 0050a5c2 */
enum { RACE_NET_FLAGS_A = 11, RACE_NET_FLAGS_B = 12 };

/* 00446528 (watcall (), from 0044655c and 00446220): both tables zeroed */
void race_net_clear_flags(int16_t a[RACE_NET_FLAGS_A], int16_t b[RACE_NET_FLAGS_B]);

enum { RACE_NET_TABLE_ENTRIES = 11, RACE_NET_WORDS_4F8138 = 8 };  /* 0050a5dc's eleven in the exe; 004f8138's dwords */

typedef struct {
    void *ctx;
    void (*transport_reset)(void *ctx);  /* 0047ab10, the transport's */
} RaceNetResetOps;

typedef struct {
    int16_t *flags_a, *flags_b;       /* 0050a5ac, 0050a5c2 */
    int32_t unknown_4bcb94;
    int32_t *unknown_4f8138;          /* RACE_NET_WORDS_4F8138 */
    RaceNetLink *entries;             /* 0050a5dc */
    uint8_t *tags;                    /* 0050a978 */
} RaceNetReset;

/* 0044655c (watcall (), from 00442970, 00442ad0 and 0045147c): 00446528,
 * 0047ab10, 004bcb94 = 1, the eight dwords at 004f8138 = 0, and 0x3ff bytes
 * from 0050a5dc zeroed (0048f1d4): the eleven entries and their eleven
 * tags. */
void race_net_reset(const RaceNetResetOps *ops, RaceNetReset *r);

/* Two buffers the network keeps at 004bc6c4: a word then a handle (unaligned) each, 004bc6ca between them untouched */
typedef struct {
    int16_t unknown_4bc6c4;
    uint32_t buffer_4bc6c6;
    int16_t unknown_4bc6cc;
    uint32_t buffer_4bc6ce;
} RaceNetBuffers;

/* 00442dec (watcall (), from 00442970 and 00442ad0): 004bc6c4 = 0, 004bc6cc =
 * 0; each handle (004bc6c6, then 004bc6ce) that is set freed (00475260) and
 * zeroed. */
typedef struct {
    void *ctx;
    void (*free_handle)(void *ctx, uint32_t handle);  /* 00475260 (cdecl) */
} RaceNetReleaseOps;

void race_net_release(const RaceNetReleaseOps *ops, RaceNetBuffers *b);

typedef struct {
    void *ctx;
    void (*close_session)(void *ctx, uint32_t session, int32_t how);  /* 004794b4 (cdecl), the transport's */
} RaceNetCloseOps;

/* The words 00442d60 reads and writes; the op may change them (the caller's ctx) */
typedef struct {
    int32_t unknown_4fbb0c;
    int16_t play;                     /* 0052b312 */
    int16_t unknown_4bc6ac;
} RaceNetClose;

/* 00442d60 (watcall (), from 00446220 and 00446284): with 004fbb0c, 0052b312
 * == 1 and 004bc6ac: 004794b4(0050a260, 0) and 004bc6ac = 0. */
void race_net_close_session(const RaceNetCloseOps *ops, RaceNetClose *c);

typedef struct {
    void *ctx;
    void (*leave)(void *ctx, uint32_t session);  /* 004795fc (cdecl), the transport's */
} RaceNetLeaveOps;

/* 00442d9c (watcall (), from 00446220): with 0052b312 == 1, 004795fc(0050a260). */
void race_net_leave(const RaceNetLeaveOps *ops, int16_t play);

typedef struct {
    void *ctx;
    void (*close_session)(void *ctx, uint32_t session, int32_t how);  /* 004794b4, through 00442d60 */
    void (*leave)(void *ctx, uint32_t session);                       /* 004795fc, through 00442d9c */
    void (*shut_down)(void *ctx);                                     /* 0047ab80, the transport's */
} RaceNetGiveUpOps;

/* The words 00446220 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t *stand_ins;               /* 004f82a0: 8 shorts */
    int32_t unknown_4bcb94;
    RaceNetClose close;               /* 004fbb0c, 0052b312, 004bc6ac */
    int16_t *flags_a, *flags_b;       /* 0050a5ac, 0050a5c2 */
    int16_t unknown_52b316;
    int32_t followed;                 /* 004fbb08 */
    int16_t dropped;                  /* 0052b314 */
} RaceNetGiveUp;

/* 00446220 (watcall (), from 004373c4, 00442ad0, 00451320 and 00451660): the
 * eight stand-ins at 004f82a0 zeroed (__STOSB, 16 bytes); then, with
 * 004bcb94: 00442d60, 00442d9c (0052b312 read anew), 0047ab80, 004bcb94 = 0,
 * 00446528, 004bc6ac = 0, 0052b316 = 0, 004fbb08 = 0, 0052b314 = 1. */
void race_net_give_up(const RaceNetGiveUpOps *ops, RaceNetGiveUp *g);

/* 004f8250: the race's settings as the host sends them (kind 7, 0x4e bytes) */
enum {
    RACE_NET_SETTINGS = 0x4e, RACE_NET_SETTINGS_KIND = 7, RACE_NET_SET_TYPE = 2, RACE_NET_SET_TRACK = 4,
    RACE_NET_SET_524EA1 = 6, RACE_NET_SET_524EA3 = 8, RACE_NET_SET_524E9D = 0xa,
    RACE_NET_SET_CARS = 0xc,       /* 004f825c: a word a slot (8), slot 0 the host's 00524e8b */
    RACE_NET_SET_READY = 0x1c,     /* 004f826c: a word a slot (12), 0052b318 */
    RACE_NET_SET_52B2B4 = 0x34,    /* 004f8284: 8 words */
    RACE_NET_SET_4C5CC2 = 0x44, RACE_NET_SET_4FBAD8 = 0x46, RACE_NET_SET_4FBADC = 0x4a,
    RACE_NET_SET_WORDS = 8, RACE_NET_SET_READIES = 12, RACE_NET_CHOICE = 6  /* a client's 7 / 00524e8b / 0052b318 */
};

typedef struct {
    void *ctx;
    void (*service)(void *ctx);                        /* 00475c14(0) */
    void (*lock)(void *ctx, uint32_t lock);            /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);          /* 00476cfc (cdecl) */
    void (*poll)(void *ctx);                           /* 00411050 */
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
    void (*menu)(void *ctx);                           /* 00466ae4, the frontend's */
} RaceNetSyncOps;

/* The words 004104d0 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t play;                     /* 0052b312 */
    int16_t host;                     /* 00524e7b */
    int16_t unknown_52b302, unknown_52b310, unknown_52b316, unknown_52b318, unknown_52b31a;
    int16_t unknown_50a5ba;           /* the settings came in (or were asked for) */
    int16_t unknown_50a5bc;           /* the host's state came in */
    uint32_t lock;                    /* 004f815c */
    uint8_t *state;                   /* 004f8164: RACE_NET_STATE bytes */
    const uint8_t *in;                /* 004f81d8: the host's state as it came in */
    uint8_t *settings;                /* 004f8250: RACE_NET_SETTINGS bytes */
    int32_t followed;                 /* 004fbb08 */
    int16_t race_type;                /* 00524e5d */
    int16_t track;                    /* 00524e85 */
    int16_t unknown_524ea1, unknown_524ea3, unknown_524e9d, unknown_4c5cc2;
    int16_t *unknown_524e8b;          /* 00524e8b (this machine's), then 00524e8d's 8 */
    int16_t *unknown_52b2b4;          /* 8 */
    int32_t unknown_4fbad8, unknown_4fbadc;
} RaceNetSync;

/* 004104d0 (watcall (), 12 callers): nothing without 0052b312; else
 * 00475c14(0) first and last (each way out below ends with it). The host
 * (00524e7b): with 0050a5bc, 0052b302 0 and 0052b312 == 1: 00411050, the
 * readies 004f826c zeroed (__STOSB, 0x18), 0050a5ba = 1, 0052b31a + 1. Else
 * with 0050a5ba 0 and the settings as the menu's: 0052b31a = 0 (a loop over
 * 004f8138 before it only sets 0052b31a to 1, overwritten). Else the settings
 * taken from the menu (kind 7 at +0), lock, 0052b2b4 into +0x34, +0xc's words
 * into 00524e8d for each human (004f81d2), 0050a5ba set: 0, 0052b31a + 1;
 * 00445e00(0, settings, 0x4e), unlock, 0052b31a + 1. A client: with
 * 0050a5bc, 0052b302 0 and 0052b312 == 1: lock, the state from 004f81d8,
 * 004fbb08 = its slot (004f81d6), 0050a5bc = 0, unlock, 0052b31a + 1. Else
 * with 0050a5ba and 0052b302 0: lock, 00466ae4 when the race type moved and
 * 0052b310, the menu's from the settings (all 8 of 00524e8d and 0052b2b4),
 * 0050a5ba = 0, unlock, 0052b31a + 1. Else when 00524e8b or 0052b318 is not
 * its slot's in the settings and 0052b316 0: both stored there and sent to
 * the host (6 bytes: 7, 00524e8b, 0052b318); else 0052b31a = 0. Port guards:
 * the humans loop stops at 8, a slot outside 0..7 sends nothing. */
void race_net_sync(const RaceNetSyncOps *ops, RaceNetSync *s);

enum { RACE_NET_STAGE_KIND = 5, RACE_NET_STAGE_FLAG = 5 /* 0050a5b6 = 0050a5ac[5] */ };

typedef struct {
    void *ctx;
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
    void (*service)(void *ctx);                        /* 00475c14(0) */
    int32_t (*step)(void *ctx);                        /* 00410bac (race_net_wait_step) */
    void (*drop)(void *ctx, int16_t slot);             /* 004463ec */
    void (*sync)(void *ctx);                           /* 004104d0 */
} RaceNetWaitOps;

/* The words 00410c18 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t play;                     /* 0052b312 */
    int16_t dropped;                  /* 0052b314 */
    int16_t unknown_52b302;
    int32_t unknown_4fbb0c;
    const uint8_t *state;             /* 004f8164: the counts at RACE_NET_AT_HUMANS / RACE_NET_AT_OTHERS */
    const uint32_t *links;            /* 0050a578: RACE_NET_LINKS */
    const RaceNetLink *entries;       /* 0050a5dc: 0..6 players, 7..10 viewers */
    int16_t *flags_a, *flags_b;       /* 0050a5ac, 0050a5c2 */
} RaceNetWait;

/* 00410c18 (watcall (stage), 6 callers): the wait for the others to reach
 * a stage. Without 0052b312: 1. With 004fbb0c: 00445e00(0, {5, stage}, 4),
 * 00475c14(0); then for each slot from 1 while below 004f81d2 + 004f81d4
 * (read each pass): while its link's entry +0 is set and 0050a5c2[slot] is
 * not the stage (both read each pass): 00410bac answering stops it and
 * drops the slot (004463ec), else with stage 1 and 0052b302 0 004104d0;
 * then 0050a5c2[slot] = 0, and 0 when 0052b314 is set; at the end 1. Else
 * while 0050a5b6 (0050a5ac[5]) is not the stage: 00410bac answering sets
 * 0052b314 = 1 and gives 0; then 0050a5b6 = 0, 00445e00(0, {5, stage}, 4),
 * 00475c14(0), 1. Port guards: a slot past the eight links ends the wait (1). */
int32_t race_net_wait(const RaceNetWaitOps *ops, RaceNetWait *w, int16_t stage);

enum { RACE_NET_WAITING_EN = 0x4b03bc, RACE_NET_WAITING_DE = 0x4b03a0 };  /* "Waiting for other players..." */

typedef struct {
    void *ctx;
    void (*sync)(void *ctx);                           /* 004104d0 */
    void (*message)(void *ctx, uint32_t text, int32_t ticks);  /* 00467214, the frontend's menu library */
    void (*deadline)(void *ctx, int32_t ticks);        /* 00410b7c */
    int32_t (*wait)(void *ctx, int16_t stage);         /* 00410c18 */
    void (*service)(void *ctx);                        /* 00475c14(0) */
} RaceNetReadyOps;

/* The words 0044633c reads; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t unknown_52b31a, unknown_50a5bc, unknown_50a5ba;
    int16_t stage_flag;               /* 0050a5b6 */
    int16_t german;                   /* 0052b31c */
    int16_t unknown_52b302;
} RaceNetReady;

/* 0044633c (watcall (), from the frontend's 00432ff4): 004104d0; then
 * only with 0052b31a, 0050a5bc and 0050a5ba 0 and 0050a5b6 == 1: the
 * waiting message (00467214(text, -1)), 00410b7c(-1), 00410c18(1); that
 * answering: with 0052b302 0, 004104d0 and 00475c14(0) until 0052b31a is
 * 0; 1. Else 0. */
int32_t race_net_ready(const RaceNetReadyOps *ops, RaceNetReady *r);

enum { RACE_NET_START_ABORTED = 0x4b03f4, RACE_NET_START_ABORTED_G = 0x4b03dc, RACE_NET_START_ABORTED_TICKS = 300 };  /* "Connection Aborted" */

typedef struct {
    void *ctx;
    void (*message)(void *ctx, uint32_t text, int32_t ticks);  /* 00467214, the frontend's menu library */
    void (*close)(void *ctx);                          /* 00442d60 (race_net_close_session) */
    void (*sync)(void *ctx);                           /* 004104d0 */
    void (*service)(void *ctx);                        /* 00475c14(0) */
    void (*deadline)(void *ctx, int32_t ticks);        /* 00410b7c */
    int32_t (*wait)(void *ctx, int16_t stage);         /* 00410c18 */
} RaceNetHostReadyOps;

/* The words 00446284 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t play;                     /* 0052b312 */
    int32_t unknown_4fbb0c;
    int16_t german;                   /* 0052b31c */
    int16_t unknown_52b302, unknown_52b31a;
    int16_t dropped;                  /* 0052b314 */
} RaceNetHostReady;

/* 00446284 (watcall (), from the frontend's 00432ff4): only with 0052b312
 * and 004fbb0c: the waiting message (00467214(text, -1)), 00442d60; with
 * 0052b302 0, 004104d0 and 00475c14(0) until 0052b31a is 0; 00410b7c(-1),
 * 00410c18(1); 0 answered: "Connection Aborted" for 300 ticks, 0052b314 = 1. */
void race_net_host_ready(const RaceNetHostReadyOps *ops, RaceNetHostReady *r);

/* The serial / modem link's entry (0050a5dc, entry 0) as 0045147c fills it */
enum {
    RACE_NET_SERIAL_HANDLER_14 = 0x451450, RACE_NET_SERIAL_9A = 0x52ac9a, RACE_NET_SERIAL_D6 = 0x52acd6,
    RACE_NET_SERIAL_FE = 0x52acfe  /* +0x50 only when 004fbb0c is 0 */
};

typedef struct {
    void *ctx;
    void (*reset)(void *ctx);                          /* 0044655c (race_net_reset) */
} RaceNetSerialOps;

/* The words 0045147c reads and writes; the op may change them (the caller's ctx) */
typedef struct {
    RaceNetLink *entry;               /* 0050a5dc */
    int16_t unknown_52ac90, unknown_52ac92, unknown_52ac94, unknown_52ac96;
    int32_t unknown_4fbb0c;
} RaceNetSerial;

/* 004109b4 (watcall (), eight calls: Control Central, the menus after a race,
 * the in-race menu): with 0052b312
 * set, 1 when any of the twelve ready words (004f826c, the settings' +0x1c)
 * is set; else 0. */
int16_t race_net_any_ready(int16_t play, const uint8_t *settings);

#endif
