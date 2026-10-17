/* The network transport's session (EA's library, 00477e94..0047bfff): a
 * session record (0050a260 for the race's) runs either the TCP layer's
 * hooks (0052cdec..0052ce20, set by 0047fab0) or its driver's methods (the
 * table at +8, eax the driver's handle +0x10). */
#ifndef TNFS_NET_SESSION_H
#define TNFS_NET_SESSION_H

#include "game/net/net_queue.h"

#include <stdint.h>

typedef struct NetServer NetServer;
struct NetServiceDriver;
typedef struct NetRequest NetRequest;

/* The session record's words read here; read again after every call */
typedef struct {
    int32_t tcp;          /* +4: not 0, the TCP layer's hooks instead of the driver's methods */
    NetServer *server;    /* +0xc: the game server's record, NULL on the clients */
    uint32_t handle;      /* +0x10: the driver's, its methods' eax */
    uint8_t *self;        /* +0x28: this machine's address (inline, 0x14 bytes) */
    NetNode node;         /* +0: its place in the sessions' list (0052cdd0) */
    uint8_t *address;     /* +0x14: the game server's address (inline) */
    int32_t joined;       /* +0x98: in a game */
    NetRequest *request;  /* +0x94: the request awaiting its answer (00477da4) */
    int32_t word_9c;      /* +0x9c: cleared with +0x98 */
    int32_t word_a0;      /* +0xa0: cleared by killnetservice */
    int32_t word_a4;      /* +0xa4: killnetservice waits for it to drop to 0 */
    uint8_t *guid;        /* +0x84: inline, 16 bytes; the game's name made from it (00477b78) */
    const struct NetServiceDriver *driver;  /* +8: the service's driver, its method table */
    char *text_3c;        /* +0x3c: inline, 0x34 bytes; initnetservice's fourth argument */
    char *text_70;        /* +0x70: inline, 0x14 bytes; its fifth */
    int32_t word_a8;      /* +0xa8: newnetgame's fourth argument */
} NetSession;

typedef struct {
    void *ctx;
    int32_t (*tcp_equal)(void *ctx, NetSession *s, const uint8_t *a, const uint8_t *b);  /* 0052ce10 (cdecl; 0048102c) */
    int32_t (*driver_equal)(void *ctx, uint32_t handle, const uint8_t *a, const uint8_t *b);  /* the driver's +0x40 */
} NetEqualOps;

enum { NET_PLAYERS = 31, NET_PLAYER_SIZE = 100, NET_PLAYER_SEEN = 0x5c };

/* The server's record (the session's +0xc); read again after every call */
struct NetServer {
    NetSession *session;  /* +4 */
    uint32_t players;     /* +0x4c: a bit for each entry in use */
    uint8_t *entries;     /* +0x94: NET_PLAYERS entries of NET_PLAYER_SIZE bytes, each the player's address first */
    int32_t word_8;       /* +8: 0, endnetgame leaves as the game server */
    uint8_t *game;        /* +0xc: inline; the driver's +0x30 gets it, its first byte cleared on leaving */
    char *password;       /* +0x3c: inline, 16 bytes */
    uint8_t *params;      /* +0x50: inline, newnetgame's 0x44 bytes (+0x40 the most players) */
};

/* 00479ad4 (cdecl (session, a, b), from 00477e94, 00478030, 00479250,
 * 00479770 and 004799a8): two network addresses the same? With +4 set the
 * TCP layer's 0052ce10 (session, a, b), else the driver's +0x40 (eax the
 * handle +0x10, edx a, ebx b); its eax returned. */
int32_t net_address_equal(const NetEqualOps *ops, NetSession *s, const uint8_t *a, const uint8_t *b);

/* 00477e94 (watcall (server, address), from 00478030 and 004799a8): the
 * player at that address. For each entry in use (+0x4c bit i, i below 31,
 * +0x4c and +4 read again each turn) 00479ad4(+4, address, the entry); the
 * first the same gets +0x5c = *now (004c62a4) and is returned; else NULL. */
uint8_t *net_find_player(const NetEqualOps *ops, NetServer *v, const uint8_t *address, const int32_t *now);

/* The words 00478e18 reads; read again after every call */
typedef struct {
    int32_t surface_locked;  /* 004c7ac0 */
    int32_t trace_level;     /* 004c62b8 */
} NetCheck;

typedef struct {
    void *ctx;
    int32_t (*main_thread)(void *ctx, uint32_t thread);              /* 00476968 (cdecl): 0 the main one's */
    void (*fatal)(void *ctx, const char *format, const char *name);  /* 00477a40 (cdecl) */
    void (*trace)(void *ctx, const char *name, uint32_t data, uint32_t text, int32_t value, int32_t flagged,
                  int32_t kind);                                     /* 0047f244 (cdecl): the trace ring */
} NetCheckOps;

extern const char NET_NOT_MAIN_THREAD[];    /* 004b2910 */
extern const char NET_SURFACE_LOCKED[];     /* 004b293c */

/* 00478e18 (watcall (name), first in 12 of the transport's calls): not on
 * the main thread (00476968(0) 0) or else with the surface locked (004c7ac0)
 * 00477a40(the message, name); then with 004c62b8 at least 1 0047f244(name,
 * 0, 0, 0, 0, 0). */
void net_check(const NetCheckOps *ops, const NetCheck *c, const char *name);

enum { NET_KILL = (int32_t)0xea6f7574 };  /* the driver's +0x54 message killnetplayer sends */

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    const NetEqualOps *equal;
    int32_t (*tcp_kill)(void *ctx, NetSession *s, const uint8_t *address);  /* 0052cdf0 (cdecl; 00480f04) */
    /* the driver's +0x54 (eax handle, edx entry, ebx message, ecx, one argument on the stack) */
    void (*driver_send)(void *ctx, uint32_t handle, uint8_t *entry, int32_t message, int32_t ecx, int32_t arg);
    void (*message)(void *ctx, const char *text);                         /* 00477a40 (cdecl) */
} NetKillOps;

extern const char NET_KILL_NOT_SERVER[];   /* 004b2ab4 */
extern const char NET_KILL_SERVER[];       /* 004b2a84 */

/* 004799a8 killnetplayer (cdecl (session, address), from 00442db8 and
 * 004465e4): 00478e18("killnetplayer"); with +4 the TCP layer's 0052cdf0
 * (session, address) returned. Else 0, after: no server (+0xc) the message
 * 004b2ab4; else the player 00477e94 finds (+0xc read again) told by the
 * driver's +0x54 (the handle, the entry, NET_KILL, 0, 0) and its bit in the
 * server's +0x4c (read after it) cleared; none, and the address this
 * machine's (00479ad4 against +0x28), the message 004b2a84. */
int32_t net_kill_player(const NetKillOps *ops, const NetCheck *c, NetSession *s, const uint8_t *address,
                        const int32_t *now);

typedef struct {
    void *ctx;
    int32_t (*clock)(void *ctx);  /* 00473324 */
} NetClockOps;

/* 00477c50 (watcall (), from 00477da4): a request's time up: 00473324()
 * less 004c62c0 (its deadline) not below 0 (signed, wrapping); 0 / 1. */
int32_t net_request_expired(const NetClockOps *ops, const int32_t *deadline);

enum { NET_BYE = (int32_t)0xea627965 };  /* the driver's +0x54 message endnetgame sends */

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    const NetMutexOps *mutex;
    NetQueue *sessions;                                                      /* 0052cdd0 */
    int32_t (*tcp_end)(void *ctx, NetSession *s);                            /* 0052ce18 (cdecl) */
    void (*driver_leave)(void *ctx, uint32_t handle, uint8_t *game);         /* the driver's +0x30 (eax, edx) */
    /* the driver's +0x54 (eax handle, edx address, ebx message, ecx, one argument on the stack) */
    void (*driver_send)(void *ctx, uint32_t handle, uint8_t *address, int32_t message, int32_t ecx, int32_t arg);
    void (*trace)(void *ctx, const char *name, const int32_t *message);    /* 0047f244 (name, &message, 0, 0, 0, 0) */
    void (*release)(void *ctx, NetServer *v);                                /* 00475260 (cdecl) */
} NetEndOps;

/* 004795fc endnetgame (cdecl (session), from 00442d9c (the race's),
 * 004790b0, 00479250 and 00479338): 00478e18("endnetgame" 004b2a40); not in
 * 0052cdd0 (0049ca1c -1): 1. +4 set: 0052ce18(session) returned. +0x98 0: 1.
 * Else, with the server (+0xc) there and its +8 0: the driver's +0x30
 * (+0x10, server + 0xc), the server's +0xc byte 0, 004c62b8 >= 2
 * 0047f244("SrvLeave->", &0xea627965, 0, ...), then for each of the 31
 * entries while +0x4c (read again) has any bit, the set ones get the driver's
 * +0x54(+0x10, the entry, 0xea627965, 0, 0) and lose their bit; 00475260
 * (server), +0xc 0. No server: 004c62b8 >= 2 0047f244("CliLeave->", ...),
 * the driver's +0x54(+0x10, session + 0x14, 0xea627965, 0, 0). Then +0x98 and
 * +0x9c 0; 1. */
int32_t net_end_game(const NetEndOps *ops, const NetCheck *c, NetSession *s);

#endif
