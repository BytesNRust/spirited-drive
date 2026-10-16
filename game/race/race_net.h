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
    void (*forget)(void *ctx, uint32_t session, uint32_t name);  /* 004799a8 (cdecl): the transport's */
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

enum { RACE_NET_SESSION = 0x50a260, RACE_NET_NAME_AT = 0x28 };

/* 00442db8 (watcall (entry), from 004463ec and 004465e4): with the word
 * 0052b312 == 1, 004799a8(0050a260, entry + 0x28), the entry's name. */
void race_net_forget(const RaceNetOps *ops, int16_t play, uint32_t entry);

/* The block 004f8164..004f81d7, sent whole (0x74 bytes) by 00411050: its
 * first word the message's kind, then the counts it carries */
enum {
    RACE_NET_STATE = 0x74, RACE_NET_STATE_KIND = 8,
    RACE_NET_AT_HUMANS = 0x6e,    /* 004f81d2 */
    RACE_NET_AT_OTHERS = 0x70,    /* 004f81d4 */
    RACE_NET_AT_TO = 0x72         /* 004f81d6: the slot it is sent to (0 for the others) */
};

typedef struct {
    void *ctx;
    void (*update)(void *ctx);                         /* 00442f58 */
    void (*lock)(void *ctx, uint32_t lock);            /* 00476c78 (cdecl) */
    void (*handle)(void *ctx);                         /* 004465e4 */
    void (*unlock)(void *ctx, uint32_t lock);          /* 00476cfc (cdecl) */
    void (*service)(void *ctx);                        /* 00475c14(0) */
    void (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 00445e00 */
} RaceNetPollOps;

/* The words 00411050 reads; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t host;                     /* 00524e7b */
    uint32_t lock;                    /* 004f815c */
    uint8_t *state;                   /* 004f8164: RACE_NET_STATE bytes */
    const uint32_t *links;            /* 0050a578 */
} RaceNetPoll;

/* 00411050 (watcall (), from 004104d0): nothing unless 00524e7b. Else
 * 00442f58; 004465e4 inside the lock 004f815c; 00475c14(0); the kind 8 at
 * 004f8164; for slot 1 while below 004f81d2 (each read anew): 004f81d6 =
 * slot, the state sent to links[slot], 00475c14(0); then 004f81d6 = 0 and for
 * slot from 004f81d2 while below 004f81d2 + 004f81d4 (read anew): the state
 * sent to links[slot], 00475c14(0). The slot is a short. */
void race_net_poll(const RaceNetPollOps *ops, const RaceNetPoll *p);

/* The newcomers' queue (0050a368 head, 0050a370 count, 0050a374 ten asks of
 * 31 bytes) that 00442f58 ("AddNewPlayers") empties: an ask's name (a string
 * at +0) goes to the 9-byte tags at 0050a978 (one an entry), the word at +9
 * picks a viewer (!= 0) or a player, the 20 bytes at +0xb the link's name */
enum {
    RACE_NET_ASKS = 10, RACE_NET_ASK = 31, RACE_NET_ASK_VIEWER = 9, RACE_NET_ASK_NAME = 0xb,
    RACE_NET_TAG = 9, RACE_NET_PLAYERS = 8, RACE_NET_VIEWERS = 4,
    RACE_NET_TAGS = RACE_NET_ENTRIES * RACE_NET_TAG,
    RACE_NET_PLAYER_OVERFLOW = 0x4b004c, RACE_NET_VIEWER_OVERFLOW = 0x4afff4  /* 00477a40's messages */
};

typedef struct {
    void *ctx;
    void (*service)(void *ctx);                                 /* 00475c14(0) */
    void (*lock)(void *ctx, uint32_t lock);                     /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);                   /* 00476cfc (cdecl) */
    void (*abort)(void *ctx, uint32_t message);                 /* 00477a40: the original ends the process */
    int32_t (*join)(void *ctx, const RaceNetLink *entry);       /* 00479a6c(entry, 0x20, 0, 0xfa, 6), cdecl */
    void (*forget)(void *ctx, uint32_t session, const uint8_t *name);  /* 004799a8 (cdecl) */
} RaceNetJoinOps;

/* The words 00442f58 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int32_t head;                     /* 0050a368 */
    int32_t count;                    /* 0050a370 */
    uint8_t asks[RACE_NET_ASKS][RACE_NET_ASK];  /* 0050a374 */
    uint32_t lock;                    /* 004f815c */
    int16_t play;                     /* 0052b312 */
    int32_t joined;                   /* 0050a5a8: the links that took */
    int16_t unknown_50a5bc;           /* set 0 at the end */
    RaceNetCounts counts;             /* 004bc6d8 / 004bc6da */
    RaceNetLink *entries;             /* 0050a5dc: RACE_NET_ENTRIES */
    uint8_t *tags;                    /* 0050a978: RACE_NET_TAGS bytes */
} RaceNetQueue;

/* 00442f58 (watcall (), from 00411050): 00475c14(0); while 0050a370 != 0
 * (read anew): the ask at the head copied, the lock 004f815c taken, head =
 * (head + 1) % 10 and count - 1 (both read after the lock), the lock left;
 * a viewer takes the first entry 7..10 whose +0 is 0 (else 00477a40 "Viewer
 * Overflow"), a player the first of 0..7 (else "Player Overflow"); the name
 * string copied to that entry's tag; 00475c14(0); 00442e3c(0050a260, the
 * ask's +0xb, slot, player) and 00479a6c on it: nonzero, 0050a5a8 + 1; zero,
 * the entry forgotten when 0052b312 == 1 (004799a8) and 004bc6d8 (player) or
 * 004bc6da (viewer) - 1. Then 0050a5bc = 0. */
void race_net_add_new_players(const RaceNetJoinOps *ops, RaceNetQueue *q);

/* The seats 004465e4 rebuilds: the state block's names (9 bytes a slot from
 * +2, this machine's at slot 0, the viewers' from slot 8), its counts, the
 * links by slot (0050a578, the entries' exe addresses; viewers past eight
 * machines run on into the four words after) and two words a slot swapped
 * when a machine's slot moves */
enum {
    RACE_NET_AT_NAMES = 2, RACE_NET_VIEWER_NAMES = 8, RACE_NET_LINK_ROOM = 12, RACE_NET_SEATS = 7,
    RACE_NET_ENTRY_SIZE = 0x54, RACE_NET_PLAYERS_AT = 0x50a5dc
};

typedef struct {
    void *ctx;
    void (*close)(void *ctx, const RaceNetLink *entry);                /* 0047b250 (cdecl), the transport's */
    void (*forget)(void *ctx, uint32_t session, const uint8_t *name);  /* 004799a8, through 00442db8 */
} RaceNetSeatOps;

typedef struct {
    int16_t host;                     /* 00524e7b */
    int16_t play;                     /* 0052b312 */
    int16_t unknown_52b310;
    const uint8_t *me;                /* 00524df0: this machine's name (a string) */
    uint8_t *state;                   /* 004f8164: RACE_NET_STATE bytes */
    uint32_t *links;                  /* 0050a578: RACE_NET_LINK_ROOM */
    const RaceNetLink *entries;       /* 0050a5dc: 0..6 players, 7..10 viewers */
    const uint8_t *tags;              /* 0050a978: RACE_NET_TAGS bytes */
    int16_t *unknown_52b2b4;          /* by slot, [-1] too (a link that had no slot) */
    int16_t *unknown_524e8d;          /* the same */
} RaceNetSeats;

/* 004465e4 (watcall (), from 00411050): nothing unless 00524e7b and 0052b312.
 * While 0052b310: each player entry's slot (0044659c) before. 004f81d6 = 0,
 * 004f81d2 = 1, this machine's name to 004f8166; for players 0..6: an entry
 * whose +0 is set goes to links[004f81d2] and its tag to the name slot
 * 004f81d2, which then grows; else with +0x20 set, 0047b250 and 00442db8 on
 * it. While 0052b310: the slots after; with 0052b310 (read once) each entry
 * whose slot moved (and has one now) swaps 0052b2b4[] and 00524e8d[] between
 * its new and old slot. 004f81d4 = 0; viewers 7..10 the same, to links[004f81d2
 * + 004f81d4] and the name slot 8 + 004f81d4. Each string copied up to its
 * NUL (port guard: within the state block and the source). */
void race_net_seat(const RaceNetSeatOps *ops, RaceNetSeats *s);

typedef struct {
    void *ctx;
    int32_t (*send)(void *ctx, uint32_t link, const uint8_t *data, int32_t size);  /* 0047b17c (cdecl), the transport's: 0 failed */
    void (*drop)(void *ctx, int16_t slot);                                         /* 004463ec */
} RaceNetSendOps;

/* The words 00445e00 reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t host;                     /* 00524e7b */
    int16_t play;                     /* 0052b312 */
    int16_t dropped;                  /* 0052b314 */
    int32_t unknown_4fbb0c;
    const uint8_t *state;             /* 004f8164: the counts at RACE_NET_AT_HUMANS / RACE_NET_AT_OTHERS */
    const uint32_t *links;            /* 0050a578: RACE_NET_LINKS */
    const RaceNetLink *entries;       /* 0050a5dc: 0..6 players, 7..10 viewers */
} RaceNetSend;

/* 00445e00 (watcall (link, data, size)): nothing once 0052b314 is set. A link
 * (an entry's exe address): sent there (0047b17c) when its +0 is set; when
 * it is not or the send fails, with 004fbb0c and 0052b312 == 1 its slot
 * (0044659c's search) dropped (004463ec), else 0052b314 = 1. No link and not
 * the host: sent to entry 0 when its +0 is set, a failure sets 0052b314. No
 * link on the host: sent to each player 0..6 and viewer 7..10 whose +0 is
 * set, each failure dropping that entry's slot (the counts and links read
 * anew). A link outside the eleven entries reads +0 as 0 (port guard). */
void race_net_send(const RaceNetSendOps *ops, RaceNetSend *s, uint32_t link, const uint8_t *data, int32_t size);

#endif
