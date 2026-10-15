/* The race's side of the network game: the waits on the other machines and
 * the words they read. The transport itself (004799a8 and on) is not here. */
#ifndef TNFS_RACE_NET_H
#define TNFS_RACE_NET_H

#include <stdint.h>

enum { RACE_NET_NO_DEADLINE = -1, RACE_NET_LINKS = 8, RACE_NET_NO_SLOT = -1 };

typedef struct {
    void *ctx;
    void (*keys)(void *ctx);     /* 0046417c: the keys read again (and the music fed) */
    void (*service)(void *ctx);  /* 00475c14(0) */
} RaceNetOps;

/* The words the waits read; `keys` may change them (the caller's ctx) */
typedef struct {
    int16_t dropped;             /* 0052b314: the network game dropped */
    int32_t deadline;            /* 004f8158: the wait's end on 004bb8e0, -1 none */
    int32_t clock;               /* 004bb8e0: the master timer's count (0043b9e8) */
} RaceNetWords;

/* Two link counts by kind (00442e3c's last argument): the host starts them
 * at 1 / 0 (00442970); a link that fails (00442f58) or ends (004460cc,
 * 004463ec) takes one off */
typedef struct {
    int16_t unknown_4bc6d8;      /* kind != 0 (the entries at 0050a5dc) */
    int16_t unknown_4bc6da;      /* kind 0 (0050a828) */
} RaceNetCounts;

/* 00410bac (watcall ()), one step of a wait: 0046417c; 1 when the game was
 * dropped (0052b314 != 0) or a deadline (not -1) is below the clock
 * (signed); else 00475c14(0) and 0. */
int32_t race_net_wait_step(const RaceNetOps *ops, const RaceNetWords *w);

/* 00442dd8 (watcall (kind)): kind (all 32 bits) != 0: 004bc6d8 - 1, else
 * 004bc6da - 1 (16-bit, wrapping) */
void race_net_count_down(RaceNetCounts *c, int32_t kind);

/* The machines' links by slot (0050a578, slot 0 this one) and the two
 * counts that bound them */
typedef struct {
    int16_t unknown_4f81d2;           /* 004f81d2 (004f81d0's high half): the humans (0042702c) */
    int16_t unknown_4f81d4;           /* 004f81d4 */
    uint32_t links[RACE_NET_LINKS];   /* 0050a578: each slot's link (a pointer) */
} RaceNetSlots;

/* 0044659c (watcall (link), from 004465e4): with n = 004f81d2 + 004f81d4
 * above 1, the first slot 1..n-1 whose link is `link`; else -1. */
int32_t race_net_slot_of(const RaceNetSlots *s, uint32_t link);

/* A link's entry (0x54 bytes). The handlers are the exe's addresses: the
 * transport that calls them is not ported. */
enum {
    RACE_NET_NAME = 20, RACE_NET_ENTRIES = 15,  /* 0050a5dc's 7 (kind != 0), then 0050a828's 8 (kind 0) */
    RACE_NET_KIND0_FIRST = 7,
    RACE_NET_HANDLER_14 = 0x443494, RACE_NET_HANDLER_LOST = 0x4460cc, RACE_NET_HANDLER_1C = 0x446058
};

typedef struct {
    uint32_t unknown_00;
    int32_t unknown_04, unknown_08, unknown_0c, unknown_10;  /* set 1, 0x19, 0xf, 10 */
    uint32_t handler_14, handler_lost, handler_1c;          /* 00443494, 004460cc (a link gone), 00446058 */
    uint32_t unknown_20;
    uint32_t owner;                                         /* +0x24: 00442ad0 / 00442f58 pass 0050a260 */
    uint8_t name[RACE_NET_NAME];                            /* +0x28 */
    uint32_t unknown_3c[6];
} RaceNetLink;

/* 00442e3c (watcall (owner, name, slot, kind), from 00442ad0 and 00442f58):
 * the entry for `slot` (a signed short) in 0050a5dc when kind (16 bits) !=
 * 0, else in 0050a828 (seven entries on, so 0050a5dc's slot 7 is 0050a828's
 * slot 0): +4..+0x10 1, 0x19, 0xf, 10, the three handlers, +0x24 the owner,
 * the name's 20 bytes at +0x28; returns it. NULL past the 15 (port guard). */
RaceNetLink *race_net_link_fill(RaceNetLink entries[RACE_NET_ENTRIES], uint32_t owner, const uint8_t *name,
                                int16_t slot, int16_t kind);

#endif
