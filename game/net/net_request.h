/* The network transport's requests to the game server (EA's library):
 * 00477da4 sends one and waits for its answer. */
#ifndef TNFS_NET_REQUEST_H
#define TNFS_NET_REQUEST_H

#include "game/net/net_packet.h"
#include "game/net/net_session.h"

#include <stdint.h>

/* A request awaiting its answer (00477da4's edx; the session's +0x94 while
 * it waits); read again after every call */
struct NetRequest {
    int32_t id;           /* +0: 004c62bc's count */
    int32_t answered;     /* +4: set by the answer's handler */
    NetSession *session;  /* +0xc */
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

#endif
