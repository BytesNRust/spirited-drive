/* The network transport's game server side (EA's library): 00478030
 * answers the requests that reach the server and the pieces it uses. */
#ifndef TNFS_NET_SERVER_H
#define TNFS_NET_SERVER_H

#include "game/net/net_request.h"
#include "game/net/net_session.h"

#include <stdint.h>

/* 00477c6c (watcall (bits), from 00478030): how many bits are set (the
 * lowest cleared, x & ~-x, until none). */
int32_t net_bit_count(uint32_t bits);

enum { NET_GAME_INFO = (int32_t)0xea646563, NET_GAME_INFO_SIZE = 0x4c };

typedef struct {
    void *ctx;
    const int32_t *trace_level;  /* 004c62b8 */
    void (*trace)(void *ctx, const char *name, const int32_t *message, uint8_t *out, int32_t size);  /* 0047f244 */
    /* the driver's +0x54 (eax handle, edx address, ebx message, ecx out, the size on the stack); its eax unused */
    int32_t (*driver_send)(void *ctx, uint32_t handle, uint8_t *address, int32_t message, uint8_t *out, int32_t size);
} NetGameInfoOps;

/* 00477c88 (watcall (server, address, id), from 00478030): an id not 0 or
 * the server's +8 0: the game's description (0x4c bytes) to the address
 * through the server's session (+4, read once): +0 the id, +4 the players'
 * bits (+0x4c), +8 00482890 (strncpy) of +0x50 (0x20), +0x28 of +0x70
 * (0x10), +0x38..+0x48 +0x80..+0x90 (0049c6a8, big-endian words); 004c62b8
 * >= 3: 0047f244("GameInfo->" 004b279c, &0xea646563, out, 0x4c, 0, 0); the
 * driver's +0x54(+0x10, address, 0xea646563, out, 0x4c). Its message word
 * sits in the saved ebp's slot, so the original returns with ebp
 * 0xea646563 (the caller's ebp, not this function's output). */
void net_game_info(const NetGameInfoOps *ops, NetServer *v, uint8_t *address, uint32_t id);

extern const char NET_ADD_NO_SLOT[];  /* 004b27b4 */

typedef struct {
    void *ctx;
    void (*message)(void *ctx, const char *text);  /* 00477a40 (cdecl) */
    const int32_t *now;                            /* 004c62a4 */
} NetAddPlayerOps;

/* 00477ef8 addnewplayer (watcall (server, record), from 00478030): the
 * first slot below the most players (+0x90, read again) less 1 (unsigned)
 * whose +0x4c bit is free: the bit set, the record's 0x5c bytes to the entry
 * (+0x94 + slot * 100), its +0x5c 004c62a4, +0x60 the slot; the slot. None:
 * 00477a40(004b27b4), 0. */
uint32_t net_add_player(const NetAddPlayerOps *ops, NetServer *v, const uint8_t *record);

/* The messages 00478030 answers, besides NET_SET_GAME (add), NET_PLAYERS_ASK
 * (req), NET_PLAYER_GET (get), NET_GAME_INFO (dec), NET_KILL (out), NET_BYE */
enum {
    NET_ACK = (int32_t)0xea61636b,          /* ack: a client alive, or the server's pulse */
    NET_BAD_PLAYER = (int32_t)0xea626164,   /* bad: no such player (getplayer's answer) */
    NET_DATA = (int32_t)0xea646174,         /* dat */
    NET_PLAYER_INFO = (int32_t)0xea706c79,  /* ply: getplayer's answer */
    NET_RESPONSE = (int32_t)0xea727370,     /* rsp: the server's answer to a request */
    NET_SERVER_ASK = (int32_t)0xea737276,   /* srv: the game asked for by its name */
    NET_ADD_ANSWER = 0xc
};

typedef struct {
    void *ctx;
    const NetEqualOps *equal;     /* 00479ad4 */
    const NetGameInfoOps *info;   /* 00477c88 */
    const NetAddPlayerOps *add;   /* 00477ef8 */
    const int32_t *trace_level;   /* 004c62b8 */
    const int32_t *now;           /* 004c62a4 */
    const int32_t *stamp;         /* 004c62a8 */
    void (*trace)(void *ctx, const char *name, const int32_t *message, uint8_t *out, int32_t size);  /* 0047f244 */
    /* the driver's +0x54 (eax handle, edx address, ebx message, ecx out, the size on the stack) */
    int32_t (*driver_send)(void *ctx, uint32_t handle, uint8_t *address, int32_t message, uint8_t *out, int32_t size);
    /* the session's +0xa8 (cdecl (session, the joining player's record, 0)): 0 turns it away */
    int32_t (*accept)(void *ctx, int32_t fn, NetSession *s, uint8_t *record);
} NetServeOps;

/* 00478030 (watcall (request, address, message, data), the size on the
 * stack, from 00477f74): a message that reached the session (the request's
 * +0xc, read once; the request's words read again). The data's words are
 * big-endian. 0047f244(name, &message, data, size, 0, 0) at 004c62b8 >= 3
 * unless said otherwise.
 *  - dec (a game's description): the request's (+0 0 and the data's id 0
 *    with +0x10 set, or +0x10 0, +0 not 0, the id +0's and 00479ad4(session,
 *    address, +0x14)); else nothing. +0x9c not 0 and +0x10 0: +0x9c =
 *    004c62a8. The game (0x44 bytes: 00482890 of +8 (0x20), of +0x28 (0x10),
 *    the words +0x38..+0x48); +8 the data's +4, +4 1; "->GameInfo"; +0x10
 *    set: it is called (session, the game, 00477c6c(+4), address), 0: +0x10 0.
 *  - get: the server (+0xc) set: an answer of +0 and +4 (as they came) and
 *    the +4th player's 0x5c bytes (-1: the session's +0x28; its +0x4c bit
 *    clear: "bad", 8 bytes only); "PlyrInfo->" with that answer, the
 *    driver's +0x54.
 *  - ply, bad: 00479ad4(session, address, +0x14) 0: "BadRspAddr"; +0x9c not
 *    0: 004c62a8; +0x14 set and the id +0's: "->PlyrInfo", for ply +0x14 is
 *    called (+0xc, data + 8, +4 -1), 0: +0x14 0; +0 0, +4 1. Else "BadPlyrReq".
 *  - rsp: 00479ad4 0: "BadSrvAddr"; the id not +0's: "BadSrvResp"; else
 *    "->Response", +4 the data's +4, +0 0, and +4 1: +0x98 004c62a4, +0x9c
 *    004c62a8.
 *  - req (session and server set): 00477c88(server, address, the id); srv:
 *    the same with id 0 when 0049b1ee (strncmp) of the server's +0xc and the
 *    data (0x20) is 0.
 *  - add (server set, its +8 0): 00477e94 finds the address: "OldPlayr->"
 *    (at 004c62b8 >= 4), the answer 1 and its slot. Else the server's bits
 *    (+0x4c) counted against its most players (+0x90, unsigned): full -2
 *    "GameFull->"; a password (+0x3c) not the data's +4 (0049b1ee, 0x10):
 *    -1 "BadPsswd->"; the address to the data's +0x14 (0x14 bytes), the
 *    session's +0xa8 set and 0 for it: -3 "DenyJoin->"; else 1 and
 *    00477ef8(server, data + 0x14) "AddPlayr->". The answer (rsp, 0xc
 *    bytes: the data's +0 as it came, the answer, the slot (not written
 *    for -1..-3: the original sends what its stack held, the port 0)) to
 *    the driver's +0x54, 0: "SrvFailRsp".
 *  - ack: server set: 00477e94, found at 004c62b8 >= 4 0047f244("ClientAlive!",
 *    the entry's +0x60, 0, 0, 0, 0). Else +0x98, +0x9c set and 00479ad4:
 *    +0x98 004c62a4, the driver's +0x54(+0x10, +0x14, ack, 0, 0), at >= 4
 *    "ClientPulse!" (&message, 0, 0).
 *  - dat: server set: 00477e94; else 00479ad4 and +0x9c set: 004c62a8.
 *  - bye, out: at 004c62b8 >= 2 "->Leave"; server set: 00477e94 found, its
 *    bit cleared, its +0x5c 0; else +0x98 and +0x9c 0.
 *  - anything else: "BadMessage". */
void net_serve(const NetServeOps *ops, NetRequest *r, uint8_t *address, int32_t message, uint8_t *data, int32_t size);

enum { NET_RECEIVE_SIZE = 0x70 };

typedef struct {
    void *ctx;
    const NetServeOps *serve;  /* 00478030's; its trace and 004c62b8 too */
    /* the driver's +0x58 (eax handle, edx address, ebx &message, ecx data, the size on the stack): the size */
    int32_t (*driver_receive)(void *ctx, uint32_t handle, uint8_t *address, int32_t *message, uint8_t *data,
                              int32_t size);
} NetReceiveOps;

/* 00477f74 (watcall (session, request), from 00478c14): until a message
 * word comes back 0: the word set to dat (0xea646174), 0048ec60 (bcopy) the
 * session's +0x28 to an address (0x14 bytes on the stack), the driver's
 * +0x58(+0x10, address, &message, a 0x70-byte buffer (kept from turn to
 * turn), 0x70) the size; a word not 0: at 004c62b8 >= 5
 * 0047f244("->NtwkRecv", &message, buffer, size, 0, 0), 00478030(request,
 * address, message, buffer, size). The session's +8 and +0x10 read again
 * each turn. */
void net_receive(const NetReceiveOps *ops, NetSession *s, NetRequest *r);

#endif
