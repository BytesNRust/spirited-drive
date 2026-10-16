/* The race's side of the network game: the waits on the other machines and
 * the words they read. The transport itself (004799a8 and on) is not here. */
#ifndef TNFS_RACE_NET_H
#define TNFS_RACE_NET_H

#include "game/race/input_queue.h"

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
    int32_t tail;                     /* 0050a36c: where 00443560 puts the next ask */
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

/* 00443560 (watcall (name, viewer, link), from the join handler 00442ec0):
 * the ask at 0050a36c gets the viewer word at +9, then the name string from
 * +0 up to its NUL (a long one runs over the word and on), then the link's
 * 20 bytes at +0xb; 0050a36c = (0050a36c + 1) % 10, 0050a370 + 1; returns
 * (0050a36c + 1) / 10. Port guards: the tail within 0..9 (else nothing, 0),
 * the string within the ten asks. */
int32_t race_net_queue_push(RaceNetQueue *q, const uint8_t *name, int16_t viewer, const uint8_t *link);

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

enum { RACE_NET_GONE_FINISH = 999999 };

typedef struct {
    void *ctx;
    void (*close)(void *ctx, uint32_t link);                     /* 0047b250 (cdecl), the transport's */
    void (*forget)(void *ctx, uint32_t session, uint32_t name);  /* 004799a8, through 00442db8 */
    void (*stand_in)(void *ctx, int16_t player);                 /* 0041019c */
} RaceNetDropOps;

/* The words 004463ec reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int32_t unknown_4fbb0c;
    int16_t unknown_4bc6ac;
    int16_t play;                     /* 0052b312 */
    int16_t dropped;                  /* 0052b314 */
    int32_t joined;                   /* 0050a5a8: the links that took */
    int16_t unknown_50a5bc;
    RaceNetCounts counts;             /* 004bc6d8 / 004bc6da */
    const uint32_t *links;            /* 0050a578: RACE_NET_LINKS */
    InputQueue *queues;               /* 004faa7c: by slot */
    PlayerRecord *records;            /* 0051cc54: by slot */
} RaceNetDrop;

/* 004463ec (watcall (slot), from 00445e00 and 004397f4): nothing for slot 0
 * or with 004fbb0c 0. The link at 0050a578[slot] is a player's (1) when it
 * is one of entries 0..6, a viewer's (0) when one of 7..10, else -1. With
 * 004bc6ac 0, a kind not 0 and the slot's queue +4 (gave up) set: nothing.
 * Else 0050a5a8 - 1, 0050a5bc = 1; with 004bc6ac 0047b250 on the link and
 * 00442db8 on it (read anew); 00442dd8(kind); then, 004bc6ac read anew and
 * 0: a kind not 0 sets the queue's gave up to 1, the slot's record finish to
 * 999999 and runs 0041019c(slot); 0050a5a8 (read anew) 0 sets 0052b314. A
 * slot below 0 or past 7: nothing (port guard; the original reads before /
 * past 0050a578, 004faa7c and 0051cc54). */
void race_net_drop(const RaceNetDropOps *ops, RaceNetDrop *d, int16_t slot);

typedef struct {
    void *ctx;
    void (*lock)(void *ctx, uint32_t lock);       /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);     /* 00476cfc (cdecl) */
    void (*stand_in)(void *ctx, int16_t player);  /* 0041019c */
} RaceNetLostOps;

/* The words 004460cc reads and writes; the ops may change them (the caller's ctx) */
typedef struct {
    int16_t host;                     /* 00524e7b */
    int16_t dropped;                  /* 0052b314 */
    uint32_t lock;                    /* 004f815c */
    int32_t joined;                   /* 0050a5a8 */
    int16_t unknown_50a5bc;
    int16_t unknown_4bc6ac;
    RaceNetCounts counts;             /* 004bc6d8 / 004bc6da */
    const uint8_t *state;             /* 004f8164: the counts at RACE_NET_AT_HUMANS / RACE_NET_AT_OTHERS */
    const uint32_t *links;            /* 0050a578: RACE_NET_LINKS */
    InputQueue *queues;               /* 004faa7c: by slot */
    PlayerRecord *records;            /* 0051cc54: by slot */
} RaceNetLost;

/* 004460cc (watcall (_, _, kind, _, link on the stack), an entry's
 * handler_lost, called by the transport): returns 0. Not the host: 0052b314
 * = 1, queue 0's gave up 1, record 0's finish 999999. The host: inside the
 * lock 004f815c (read anew for the unlock), 0050a5a8 - 1, 0050a5bc = 1; with
 * 004bc6ac 0 the link's slot (0044659c's search) below 004f81d2 gets gave up
 * 1, finish 999999 and 0041019c(slot), then 0050a5a8 (read anew) 0 sets
 * 0052b314; then kind becomes 1 when the link is one of entries 0..6, 0 when
 * one of 7..10 (else it stays) and 00442dd8(kind). Port guard: a link with
 * no slot (-1) marks nobody (the original writes before the queues and
 * records and runs 0041019c(-1)). */
int32_t race_net_link_lost(const RaceNetLostOps *ops, RaceNetLost *l, int32_t kind, uint32_t link);

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

#endif
