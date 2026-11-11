/* part 4: the handlers of the messages that arrive (state, settings, stage, hello, ping, info, setup, chat), the receive and the escapes. (split from race_net.h; race_net.h includes all four.) */
#ifndef TNFS_RACE_NET_IN_H
#define TNFS_RACE_NET_IN_H

#include "game/race/race_net_links.h"
#include "game/race/race_net_session.h"
#include "game/race/race_net_start.h"

#include <stdint.h>

enum { RACE_NET_STANDIN_BLOCK = 0x12 };  /* 004f829e: the kind word, then the eight stand-ins (004f82a0) */

typedef struct {
    void *ctx;
    int16_t (*slot_of)(void *ctx, uint32_t link);      /* 0044659c */
    void (*stand_in)(void *ctx, int16_t player);       /* 0041019c */
} RaceNetStandInOps;

/* The words handler 10 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int32_t unknown_4fbb0c;
    uint8_t *block;                   /* 004f829e: RACE_NET_STANDIN_BLOCK bytes */
    int32_t racers;                   /* 004fb7dc */
    PlayerRecord *records;            /* 0051cc54: by slot */
} RaceNetStandIns;

/* 00410324, message kind 10 (the handler table 004bcb98; eax link, edx
 * data): with 004fbb0c, 0041019c(0044659c(link)); else the 0x12 bytes into
 * 004f829e. Then each racer below 004fb7dc whose stand-in word is set
 * finishes at 999999. Port guard: eight racers at most. */
void race_net_on_stand_ins(const RaceNetStandInOps *ops, RaceNetStandIns *s, uint32_t link, const uint8_t *data);

typedef struct {
    void *ctx;
    void (*lock)(void *ctx, uint32_t lock);            /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);          /* 00476cfc (cdecl) */
} RaceNetLockOps;

/* The words handler 8 writes; the ops may change them (the caller's ctx) */
typedef struct {
    uint32_t lock;                    /* 004f815c */
    uint8_t *in;                      /* 004f81d8: RACE_NET_STATE bytes */
    int16_t unknown_50a5bc;
} RaceNetStateIn;

/* 00410384, message kind 8 (eax link, edx data): lock, the host's 0x74-byte
 * state into 004f81d8, 0050a5bc = 1, unlock (004f815c read anew). */
void race_net_on_state(const RaceNetLockOps *ops, RaceNetStateIn *s, const uint8_t *data);

typedef struct {
    void *ctx;
    void (*lock)(void *ctx, uint32_t lock);            /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);          /* 00476cfc (cdecl) */
    int16_t (*slot_of)(void *ctx, uint32_t link);      /* 0044659c */
} RaceNetSettingsInOps;

/* The words handler 7 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    uint32_t lock;                    /* 004f815c */
    int16_t host;                     /* 00524e7b */
    uint8_t *settings;                /* 004f8250: RACE_NET_SETTINGS bytes */
    int32_t *unknown_4f8138;          /* RACE_NET_WORDS_4F8138 */
    int16_t unknown_50a5ba;
} RaceNetSettingsIn;

/* 004103c4, message kind 7 (eax link, edx data): lock, then 00524e7b (read
 * after it): the host takes the sender's car (data +2) and ready word (+4)
 * into its slot (short) 0044659c(link) of the settings and sets the slot's
 * 004f8138 dword to 1; a client takes the whole block but its kind word
 * (+2..+0x4e). 0050a5ba = 1, unlock (004f815c read anew). Port guard: a slot
 * outside 0..7 stores nothing (the original writes around the arrays). */
void race_net_on_settings(const RaceNetSettingsInOps *ops, RaceNetSettingsIn *s, uint32_t link, const uint8_t *data);

typedef struct {
    void *ctx;
    int16_t (*slot_of)(void *ctx, uint32_t link);      /* 0044659c */
} RaceNetSlotOps;

/* The words handler 5 reads and writes */
typedef struct {
    int16_t host;                     /* 00524e7b */
    int16_t *flags_a, *flags_b;       /* 0050a5ac, 0050a5c2 */
} RaceNetStageIn;

/* 00410bf0, message kind 5 (eax link, edx data): the stage another machine
 * reached (data +2). The host stores it at 0050a5c2[(short) 0044659c(link)],
 * a client at 0050a5b6 (0050a5ac[5]). Port guard: a slot outside 0..11
 * stores nothing. */
void race_net_on_stage(const RaceNetSlotOps *ops, RaceNetStageIn *s, uint32_t link, const uint8_t *data);

/* The words handler 9 reads and writes */
typedef struct {
    int32_t unknown_4fbb0c;           /* this machine's side */
    int16_t dropped;                  /* 0052b314 */
    int32_t unknown_513e44;           /* both machines took the same side */
    int16_t unknown_50a5be;           /* the other's hello came in */
    uint8_t *state;                   /* 004f8164: RACE_NET_STATE bytes */
    uint8_t *tags;                    /* 0050a978: RACE_NET_TAGS bytes */
} RaceNetHelloIn;

/* 0045156c, message kind 9 (eax link, edx data: 00451660's hello): the
 * sender's side (the short at +2) equal to 004fbb0c (all 32 bits): 0052b314
 * = 1 and 00513e44 = 1. Else 0050a5be = 1 and the sender's name (+4) copied
 * to its NUL into the state block's name 004fbb0c, and with 004fbb0c not 0
 * also into 0050a978 (entry 0's tag). Port guard: the name within the state
 * block, and within RACE_NET_TAG bytes of the hello and of the tag. */
void race_net_on_hello(RaceNetHelloIn *h, const uint8_t *data);

enum {
    RACE_NET_PING_KIND = 4, RACE_NET_PING = 4,
    RACE_NET_PINGS = 12,        /* 004f8108's dwords, up to 004f8138 */
    RACE_NET_PINGED_FLAG = 4    /* 0050a5b4 = 0050a5ac[4] */
};

typedef struct {
    void *ctx;
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
    int16_t (*slot_of)(void *ctx, uint32_t link);                              /* 0044659c */
} RaceNetPingOps;

/* The words handler 4 reads and writes */
typedef struct {
    int16_t *flags_a;                 /* 0050a5ac */
    int32_t clock;                    /* 004bb8e0 */
    int32_t *unknown_4f8108;          /* RACE_NET_PINGS: the clock a machine last answered at */
} RaceNetPingIn;

/* 00410f90, message kind 4 (eax link, edx data): by the word at +2 (unsigned;
 * past 3 nothing): 0 and 1 answer the sender (00445e00(link, {4, 1 / 2}, 4));
 * 2 sets 0050a5b4 (0050a5ac[4]) = 1 and answers {4, 3}; 3 stores 004bb8e0 at
 * 004f8108[(short) 0044659c(link)]. Port guard: a slot outside 0..11 stores
 * nothing. */
void race_net_on_ping(const RaceNetPingOps *ops, RaceNetPingIn *p, uint32_t link, const uint8_t *data);

enum { RACE_NET_PINGS_CLEARED = 16, RACE_NET_PING_WAIT = 400 };  /* 004f8108's dwords 00410d80 may clear (into 004f8138's) */

typedef struct {
    void *ctx;
    void (*service)(void *ctx);                                                 /* 00475c14(0) */
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
    int32_t (*step)(void *ctx);                                                 /* 00410bac (race_net_wait_step) */
    void (*drop)(void *ctx, int16_t slot);                                      /* 004463ec */
    int32_t (*latency)(void *ctx, int32_t ticks);                               /* 00411160 */
} RaceNetPingAllOps;

/* Read again after every call (the ctx may change them) */
typedef struct {
    int32_t unknown_4fbb0c;
    const uint8_t *state;             /* 004f8164: the counts at RACE_NET_AT_HUMANS / RACE_NET_AT_OTHERS */
    const uint32_t *links;            /* 0050a578: RACE_NET_LINKS */
    const RaceNetLink *entries;       /* 0050a5dc: 0..6 players, 7..10 viewers */
    int32_t *pings;                   /* 004f8108: RACE_NET_PINGS_CLEARED */
    int32_t clock;                    /* 004bb8e0 */
    int32_t deadline;                 /* 004f8158 */
    int16_t *flags_a;                 /* 0050a5ac ([4] 0050a5b4: pinged) */
    int16_t dropped;                  /* 0052b314 */
    int32_t unknown_4fbac8;           /* written: 00411160 of the result */
} RaceNetPingAll;

/* 00410d80 (watcall (), from 004273d0). A client (004fbb0c 0): 00475c14(0),
 * 004f8158 = -1, until 0050a5b4: 00410bac answering: 0052b314 = 1, -2;
 * 00475c14(0). Then 0050a5b4 = 0, 1. The host: 004f8108's first 2 x humans
 * dwords 0; start = 004bb8e0; 00445e00(0, {4, 0}, 4); until = 004bb8e0 +
 * 400; while until > 004bb8e0 (signed, from the start only if so): for slot
 * 1.. below humans + others, 00475c14(0) each: a slot with a link (its
 * entry's +0) and no answer (004f8108[slot] 0) ends the round; the last
 * slot reached ends the wait (until = 0); then 00475c14(0). Then for slot 1..:
 * 00475c14(0); answered: (answer - start) / 2 (toward zero), the largest
 * kept; else 004463ec(slot). None answered: -1; else 004fbac8 =
 * 00411160(largest), largest. Port guards: 16 dwords cleared, 8 slots. */
int32_t race_net_ping_all(const RaceNetPingAllOps *ops, RaceNetPingAll *p);

/* 00411160 (watcall (ticks)): the queue's wait (004fbac8) for the slowest
 * answer, from 00410020's 51 dwords at ticks clamped to 0..50: 2 below 8,
 * 4 below 22, 6 below 34, else 8. */
int32_t race_net_latency(int32_t ticks);

enum { RACE_NET_INFO = 0x4b, RACE_NET_INFO_STAGE = 4 };  /* a player's record at 004fb7f8; 0050a5c2's word after it */

/* The words handler 0 writes */
typedef struct {
    int16_t *flags_b;                 /* 0050a5c2 */
    uint8_t *infos;                   /* 004fb7f8: RACE_NET_INFO bytes a player (8) */
} RaceNetInfoIn;

/* 00439c5c, message kind 0 (eax link, edx data): slot = (short)
 * 0044659c(link); the 0x4b bytes from the data (its kind word too) into
 * 004fb7f8 + 0x4b x slot, then 0050a5c2[slot] = 4. Port guard: the record
 * for a slot in 0..7 only, the word for one in 0..11. */
void race_net_on_info(const RaceNetSlotOps *ops, RaceNetInfoIn *in, uint32_t link, const uint8_t *data);

typedef struct {
    void *ctx;
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
} RaceNetInfoSendOps;

/* 00439c30 (eax record, from 004273d0): with 0052b312 set and 0052b316 0, the
 * record's first word = 0 (kind 0) and 00445e00(0, record, 0x4b). */
void race_net_send_info(const RaceNetInfoSendOps *ops, int16_t play, int16_t replay, uint8_t *record);

enum {
    RACE_NET_SETUP = 0x348,           /* 004fb7bc: the race's setup (REPLAY_SETUP_SIZE) */
    RACE_NET_SETUP_CHUNK = 0xf7,      /* a message's share: {1, chunk byte, its bytes} */
    RACE_NET_SETUP_HEAD = 3,
    RACE_NET_SETUP_END = 2,           /* the size of the last message, {1} */
    RACE_NET_SETUP_4FBACC = 0x310,
    RACE_NET_SETUP_TRAILER_CHUNK = 4,   /* port: the chunk after the setup's four (0xf7 x 4 is past its end, so a receiver that knows no trailer drops it by its guard) */
    RACE_NET_SETUP_TRAILER_MAX = RACE_NET_SETUP_CHUNK,
    RACE_NET_SETUP_FLAG = 1           /* 0050a5ae = 0050a5ac[1] */
};

/* The words handler 1 reads and writes */
typedef struct {
    int16_t *flags_a;                 /* 0050a5ac */
    const uint8_t *state;             /* 004f8164: RACE_NET_STATE bytes */
    uint8_t *setup;                   /* 004fb7bc: RACE_NET_SETUP bytes */
    uint8_t *trailer;                 /* port: RACE_NET_SETUP_TRAILER_MAX bytes for the host's trailer (session_rules.h); NULL: none taken */
    int32_t *trailer_size;            /* port: its size, 0 when this setup came with none */
} RaceNetSetupIn;

/* 00439d98, message kind 1 (eax link, edx data, ebx size): size 2 (all 32
 * bits): 0050a5ae = 1 and the dword 004fbacc = the short 004f81d6. Else
 * size - 3 bytes from data +3 into the setup at 0xf7 x the byte at +2. Port
 * guard: the copy within the setup, nothing for a size below 3 (the
 * original's count wraps). */
void race_net_on_setup(RaceNetSetupIn *in, const uint8_t *data, int32_t size);

typedef struct {
    void *ctx;
    void (*service)(void *ctx);                                                 /* 00475c14(0) */
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
} RaceNetSetupSendOps;

/* 00439de0 (eax setup, from 004273d0 with 004fb7bc): with the setup's dword
 * +0x18 (004fb7d4) 1: 004fbacc (+0x310) = the short 004f81d6, the setup's
 * first word = 1; then {1, 0, chunk, up to 0xf7 bytes of the setup} for
 * chunks 0, 1, ... (00475c14(0) before each 00445e00(0, message, n + 3)),
 * and last 00445e00(0, message, 2), what handler 1 (00439d98) takes. */
void race_net_send_setup(const RaceNetSetupSendOps *ops, uint8_t *setup, const uint8_t *state);
/* port: the same, with a trailer (up to RACE_NET_SETUP_TRAILER_MAX bytes) as chunk RACE_NET_SETUP_TRAILER_CHUNK after the setup's, before the end */
void race_net_send_setup_trailer(const RaceNetSetupSendOps *ops, uint8_t *setup, const uint8_t *state, const uint8_t *trailer, int32_t trailer_size);

enum { RACE_NET_MENU_NAME = 0x95 };  /* 0052ad44's names, one a player */

typedef struct {
    void *ctx;
    void (*deadline)(void *ctx, int32_t ticks);      /* 00410b7c */
    int32_t (*step)(void *ctx);                      /* 00410bac (race_net_wait_step) */
    void (*names_done)(void *ctx, int32_t humans);   /* 00466b90, the frontend's */
} RaceNetSetupWaitOps;

/* Read again after every call (the ctx may change them) */
typedef struct {
    int16_t play;                     /* 0052b312 */
    int16_t dropped;                  /* 0052b314 */
    int16_t *flags_a;                 /* 0050a5ac ([1] 0050a5ae: the setup came in) */
    int32_t humans;                   /* 004fb7e0 */
    const uint8_t *records;           /* 004fb7f8: RACE_NET_INFO bytes a player, the name at +2 */
    uint8_t *names;                   /* 0052ad44: RACE_NET_MENU_NAME bytes a player */
} RaceNetSetupWait;

/* 00439ea0 (watcall (), from 004273d0): with 0052b312: 00410b7c(-1); until
 * 0050a5ae: 00410bac, answering: 0052b314 = 1 and on; else 0052b312 gone:
 * return. Then 0050a5ae = 0, each of the 004fb7e0 humans' names (record +2)
 * to 0052ad44 + 0x95 i, 00466b90(004fb7e0). Port guard: 8 names at most,
 * each within the record. */
void race_net_wait_setup(const RaceNetSetupWaitOps *ops, RaceNetSetupWait *w);

enum {
    RACE_NET_SETUP_RECORD_A = 0x3c,   /* 004fb7f8: record 0 (RACE_NET_INFO bytes) */
    RACE_NET_SETUP_RECORD_B = 0x87,   /* 004fb843: record 1 */
    RACE_NET_SETUP_RACERS = 0x20,     /* 004fb7dc */
    RACE_NET_SETUP_HUMANS = 0x24,     /* 004fb7e0 */
    RACE_NET_SETUP_SUM = 0x2c,        /* 004fb7e8 = +0x28 + +0x20 + +0x30 */
    RACE_NET_SETUP_SUM_A = 0x28, RACE_NET_SETUP_SUM_B = 0x30,
    RACE_NET_SEATED = 4               /* 0050a5c2[slot]: the slot's info came in */
};

typedef struct {
    void *ctx;
    void (*service)(void *ctx);                      /* 00475c14(0) */
    void (*deadline)(void *ctx, int32_t ticks);      /* 00410b7c */
    int32_t (*step)(void *ctx);                      /* 00410bac (race_net_wait_step) */
    void (*drop)(void *ctx, int16_t slot);           /* 004463ec */
    void (*names_done)(void *ctx, int32_t humans);   /* 00466b90, the frontend's */
} RaceNetSetupHostOps;

/* Read again after every call (the ctx may change them) */
typedef struct {
    uint8_t *setup;                   /* 004fb7bc: RACE_NET_SETUP bytes (the records and 004fb7e0 in it) */
    const uint8_t *state;             /* 004f8164: the humans at RACE_NET_AT_HUMANS (004f81d2) */
    const uint32_t *links;            /* 0050a578: RACE_NET_LINKS */
    const RaceNetLink *entries;       /* 0050a5dc */
    int16_t *flags_b;                 /* 0050a5c2: RACE_NET_FLAGS_B */
    uint8_t *names;                   /* 0052ad44: RACE_NET_MENU_NAME bytes a player */
} RaceNetSetupHost;

/* 00439c98 (eax setup, edx record, ebx record; from 004273d0 with 004fb7bc):
 * 00475c14(0); the setup's +0x2c = +0x28 + +0x20 + +0x30; the first record
 * (RACE_NET_INFO bytes) to +0x3c. With +0x18 (004fb7d4) not 1: the second to
 * +0x87, done. Else 00410b7c(-1); each slot 1 .. the short 004f81d2 - 1 (read
 * each time) whose link's first dword is set: 00410bac until 0050a5c2[slot]
 * is 4, an answer then 004463ec(slot); 0050a5c2[slot] = 0. Then each of the
 * 004fb7e0 humans' names (record +2) to 0052ad44 + 0x95 i, 00466b90(004fb7e0).
 * Port guard: 8 slots, 8 names, each within its record. */
void race_net_host_setup(const RaceNetSetupHostOps *ops, RaceNetSetupHost *h, const uint8_t *first, const uint8_t *second);

/* 00439f54 (eax setup, edx record; from 004273d0 with 004fb7bc once 00513e58
 * is set): the setup's +0x2c = +0x28 + +0x20 + +0x30; the record (RACE_NET_INFO
 * bytes) to +0x3c and to +0x87. */
void race_net_setup_both(uint8_t *setup, const uint8_t *record);

typedef struct {
    void *ctx;
    void (*setup_both)(void *ctx, uint8_t *setup, const uint8_t *record);                          /* 00439f54 */
    void (*host_setup)(void *ctx, uint8_t *setup, const uint8_t *first, const uint8_t *second);    /* 00439c98 */
    void (*send_setup)(void *ctx, uint8_t *setup);                                                  /* 00439de0 */
    void (*keys)(void *ctx);                                                                        /* 0046417c, the frontend's */
    void (*send_info)(void *ctx, uint8_t *record);                                                  /* 00439c30 */
    void (*wait_setup)(void *ctx);                                                                  /* 00439ea0 */
} RaceNetExchangeOps;

/* Read again after every call (the ctx may change them) */
typedef struct {
    int32_t once;                     /* 00513e58: the setup went out once (a solo race's) */
    int32_t unknown_4fbb0c;           /* the host's */
    int16_t play;                     /* 0052b312 */
    uint8_t *setup;                   /* 004fb7bc: RACE_NET_SETUP bytes (004fb7d4 at +0x18) */
} RaceNetExchange;

/* 004273d0 (eax record, edx record): with 00513e58: 00439f54(004fb7bc, the
 * first), done. The host (004fbb0c): 00439c98(004fb7bc, first, second); else
 * 0046417c and 00439c30(first). Then with 0052b312 0 and 004fb7d4 1:
 * 00513e58 = 1, done; else the host 00439de0(004fb7bc), a client 00439ea0;
 * and again with 0052b312 0 and 004fb7d4 1: 00513e58 = 1. */
void race_net_exchange_setup(const RaceNetExchangeOps *ops, RaceNetExchange *x, uint8_t *first, uint8_t *second);

enum {
    RACE_NET_CHAT = 0xdf,             /* {6, the sender's name (+2), its text (+0xb), more text (+0x75)} */
    RACE_NET_CHAT_NAME = 2, RACE_NET_CHAT_TEXT = 0xb, RACE_NET_CHAT_MORE = 0x75,
    RACE_NET_CHAT_LINES = 9, RACE_NET_CHAT_LINE = 0x277  /* 004b7a30's lines */
};

typedef struct {
    void *ctx;
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
    void (*lock)(void *ctx, uint32_t lock);            /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);          /* 00476cfc (cdecl) */
} RaceNetChatOps;

/* The words handler 6 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t host;                     /* 00524e7b */
    uint32_t lock;                    /* 004f824c */
    uint8_t *lines;                   /* 004b7a30: RACE_NET_CHAT_LINES x RACE_NET_CHAT_LINE */
    int16_t line;                     /* 004b905f: the next line to write */
    int16_t unknown_4b9062;           /* a line came in */
} RaceNetChatIn;

/* 0040a3f0, message kind 6 (eax link, edx data: a chat message): with
 * 00524e7b and a link not 0, the 0xdf bytes relayed (00445e00(0, a stack
 * copy, 0xdf)). lock(004f824c); then (004b905f read after it) the line takes
 * the name (+2), the next "   " and the text (+0xb), and with a byte at +0x75
 * the next "   " and that text; the line count goes on by one after each,
 * back to 0 at exactly 9. 004b9062 = 1, unlock (004f824c read anew). Port
 * guard: a line outside 0..8 is not written, the strings kept within it. */
void race_net_on_chat(const RaceNetChatOps *ops, RaceNetChatIn *c, uint32_t link, const uint8_t *data);

enum { RACE_NET_KINDS = 11, RACE_NET_BUG_TEXT = 0x4b0370 };  /* 004bcb98's handlers; "BUG: Unknow Packet Index Received <%d> pc<%p>" */

typedef struct {
    void *ctx;
    /* 004bcb98[kind] (eax link, edx data, ebx size): 0 00439c5c race_net_on_info, 1 00439d98 race_net_on_setup,
     * 2 004395cc input_on_message, 3 004109e4 race_schedule_on_message, 4 00410f90 race_net_on_ping,
     * 5 00410bf0 race_net_on_stage, 6 0040a3f0 race_net_on_chat, 7 004103c4 race_net_on_settings,
     * 8 00410384 race_net_on_state, 9 0045156c race_net_on_hello, 10 00410324 race_net_on_stand_ins */
    void (*handle)(void *ctx, int16_t kind, uint32_t link, const uint8_t *data, int32_t size);
    /* 00473d43(buffer, text, kind, link), then 00477a40(buffer): the fatal error */
    void (*bug)(void *ctx, uint32_t text, int32_t kind, uint32_t link);
} RaceNetReceiveOps;

/* 00446058 (cdecl (link, data, size), the transport's receive callback):
 * 1 always. With 0052b314 (dropped) nothing; else the kind (the word at
 * data, signed) below 11 goes to its handler, past it to the BUG abort.
 * Port guard: a negative kind does nothing (the original calls through
 * the words before the table). */
int32_t race_net_receive(const RaceNetReceiveOps *ops, int16_t dropped, uint32_t link, const uint8_t *data, int32_t size);

typedef struct {
    void *ctx;
    void (*wait_release)(void *ctx);  /* port: the original spins on the key's flag as read (a hang while it is down) */
} RaceNetEscapeOps;

/* 00451450 (the transport's escape callback, set by 0045147c): Esc (key 1,
 * 004c693d) up: 0. Down: 00513e48 = 1, the key's flag read again and spun
 * on (wait_release), 1. */
int32_t race_net_link_escape(const RaceNetEscapeOps *ops, uint8_t esc, int32_t *unknown_513e48);

typedef struct {
    void *ctx;
    void (*service)(void *ctx);       /* 0046417c, the frontend's */
} RaceNetSessionEscapeOps;

/* Read again after every call (the service moves the keys on) */
typedef struct {
    uint8_t esc;                      /* 004c693d: key 1 */
    int32_t unknown_50a2fc;
} RaceNetSessionEscape;

/* 00443494 (a transport callback, no arguments): 0046417c; Esc down:
 * 0046417c again, then while Esc is down 0046417c, and 1. Esc up: 1 when
 * 0050a2fc is 0, else 0. */
int32_t race_net_session_escape(const RaceNetSessionEscapeOps *ops, const RaceNetSessionEscape *e);

#endif
