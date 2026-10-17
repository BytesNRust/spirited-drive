/* The network transport's requests to the game server (EA's library):
 * 00477da4 sends one and waits for its answer. */
#ifndef TNFS_NET_REQUEST_H
#define TNFS_NET_REQUEST_H

#include "game/net/net_packet.h"
#include "game/net/net_session.h"

#include <stdint.h>

/* 00479770's callback (cdecl (session, the player's record, 1 for the
 * session's own +0x28)): 0 stops it */
typedef struct {
    void *ctx;
    int32_t (*fn)(void *ctx, NetSession *s, const uint8_t *record, int32_t self);
} NetPlayerCallback;

/* A request awaiting its answer (00477da4's edx; the session's +0x94 while
 * it waits); read again after every call */
struct NetRequest {
    int32_t id;           /* +0: 004c62bc's count */
    int32_t answered;     /* +4: set by the answer's handler (-3: no such player) */
    uint32_t players;     /* +8: getnetplayers' answer, a bit for each player */
    NetSession *session;  /* +0xc */
    const void *games;    /* +0x10: getnetgames' callback, cleared to end its wait */
    const NetPlayerCallback *callback;  /* +0x14: getnetplayers' */
};

typedef struct {
    void *ctx;
    const NetClockOps *clock;   /* 00473324 */
    const NetWaitOps *wait;     /* 0047337d's service */
    const int32_t *wait_clock;  /* 0052eebc, 0047337d's */
    int32_t *next_id;           /* 004c62bc */
    int32_t *deadline;          /* 004c62c0 */
    const int32_t *trace_level; /* 004c62b8 */
    const int32_t *resend;      /* 004c62ac: the ticks after a send that went */
    void (*trace)(void *ctx, const char *name, const int32_t *message, uint8_t *out, int32_t size);  /* 0047f244 */
    /* the driver's +0x48 (eax handle, edx address), NULL when the table's +0x48 is 0 */
    void (*driver_prepare)(void *ctx, uint32_t handle, uint8_t *address);
    /* the driver's +0x54 (eax handle, edx address, ebx message, ecx out, one argument on the stack) */
    int32_t (*driver_send)(void *ctx, uint32_t handle, uint8_t *address, int32_t message, uint8_t *out, int32_t size);
} NetRequestOps;

/* 00477da4 (watcall (session, request, message, out, size, timeout), from
 * 00479250 and 00479770): +0xc the session, +0 004c62bc (counted up), +4 0;
 * 0049c6a8(out, the id, 4); 004c62b8 >= 3: 0047f244("SendSrvReq", &message,
 * out, size, 0, 0); the driver's +0x48 (+0x10, session + 0x14) if set; the
 * session's +0x94 the request; 004c62c0 = 00473324() + timeout. While +4 is
 * 0 and 00477c50 0: the driver's +0x54(+0x10, session + 0x14, message, out,
 * size), then 0047337d(004c62ac) when it answered not 0, else 0047337d(1).
 * The session's +0x94 0. */
void net_request_send(const NetRequestOps *ops, NetSession *s, NetRequest *r, int32_t message, uint8_t *out,
                      int32_t size, int32_t timeout);

enum { NET_SET_GAME = (int32_t)0xea616464, NET_SET_SIZE = 0x70, NET_PASSWORD = 0x10, NET_ADDRESS = 0x14,
       NET_RECORD = 0x5c };

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    const NetEqualOps *equal;
    const NetEndOps *end;
    const NetRequestOps *request;
    int32_t (*tcp_set)(void *ctx, NetSession *s, const uint8_t *address, const char *password);  /* 0052cdf8 (cdecl) */
    const int32_t *timeout;  /* 004c6170: 100, doubled */
} NetSetOps;

/* 00479250 setnetgame (cdecl (session, address, password), from 00442ad0):
 * 00478e18("setnetgame" 004b29b4); +4 set: 0052cdf8(session, address,
 * password) returned. Else a request from 00477b30's 24 zero bytes; with
 * +0x98 set, 00479ad4(session, +0x14, address) not 0: 1, else 004795fc
 * (session) and +0x98 0. The out buffer (0x70 bytes): +0x14 on the 0x5c
 * bytes from +0x28, +0x14..+0x27 the address's 0x14 bytes, +4 00482890
 * (strncpy) of the password, 16 bytes; then 00477da4(session, &request,
 * 0xea616464, out, 0x70, 004c6170 * 2) and the request's +4 returned. */
int32_t net_set_game(const NetSetOps *ops, const NetCheck *c, NetSession *s, const uint8_t *address,
                     const char *password);

enum { NET_PLAYERS_ASK = (int32_t)0xea726571, NET_PLAYERS_SIZE = 0x4c, NET_PLAYER_GET = (int32_t)0xea676574,
       NET_PLAYER_SIZE_OUT = 8 };

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    const NetEqualOps *equal;
    const NetRequestOps *request;
    int32_t (*tcp_players)(void *ctx, NetSession *s, const uint8_t *address, const NetPlayerCallback *cb);  /* 0052ce0c (cdecl) */
    const int32_t *timeout;  /* 004c6170: doubled */
} NetPlayersOps;

/* 00479770 getnetplayers (cdecl (session, address, callback), from 004428fc):
 * 00478e18("getnetplayers" 004b2a64); +4 set: 0052ce0c(session, address,
 * callback) returned. No address: +0x14 with +0x98 set, else 0; an address
 * with +0x98 set: 0; else its 0x14 bytes to +0x14. A server (+0xc) and
 * 00479ad4(session, +0x14, address) not 0: callback(session, +0x28, 1), then
 * for the 31 entries whose bit of the server's +0x4c (both read again) is
 * set, callback(session, the entry, 0), counted until one gives 0. Else a
 * request from 00477b48's 24 zero bytes: 00477da4(0xea726571, out, 0x4c,
 * 004c6170 * 2); its +4 0: 0. Else +0x14 the callback, +4 1, and for i from
 * -1 to 31 while +4 is not 0, i -1 or its bit set in +8 (as the first
 * answer left it): 0049c6a8(out + 4, i, 4), 00477da4(0xea676574, out, 8,
 * 004c6170 * 2); +4 -3: 0, else counted. The count returned. */
int32_t net_get_players(const NetPlayersOps *ops, const NetCheck *c, NetSession *s, const uint8_t *address,
                        const NetPlayerCallback *cb);

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    const NetClockOps *clock;   /* 00473324 */
    const NetWaitOps *wait;     /* 0047337d's service */
    const int32_t *wait_clock;  /* 0052eebc, 0047337d's */
    int32_t *deadline;          /* 004c62c0 */
    int32_t (*tcp_games)(void *ctx, NetSession *s, const void *callback, int32_t timeout);  /* 0052cdfc (cdecl) */
    int32_t (*driver_find)(void *ctx, uint32_t handle, const char *name);  /* the driver's +0x34 (eax, edx) */
    void (*driver_stop_find)(void *ctx, uint32_t handle);                   /* the driver's +0x38 (eax) */
} NetGamesOps;

/* 00479174 getnetgames (cdecl (session, callback, timeout), from 00442684):
 * 00442684): 00478e18("getnetgames" 004b29a8); +4 set: 0052cdfc
 * (session, callback, timeout) returned. Else 00477b78(+0x84) names the game
 * (48 bytes on the stack); a request from 00477b18's 24 zero bytes, +0xc the
 * session, +0 0, +0x10 the callback, the session's +0x94; 004c62c0 =
 * 00473324() + timeout. Until the driver's +0x34(+0x10, the name) answers not
 * 0 it is asked again each turn; 0047337d(1); on while 00473324() less
 * 004c62c0 is below 0 and +0x10 is not 0. Its answer not 0: the driver's
 * +0x38(+0x10). +0x94 0; the answer returned. */
int32_t net_get_games(const NetGamesOps *ops, const NetCheck *c, NetSession *s, const void *callback,
                      int32_t timeout);

#endif
