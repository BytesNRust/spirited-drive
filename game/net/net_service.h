/* The network transport's services (EA's library): a session record's
 * life in the sessions' list (0052cdd0); 004790b0 ends one. */
#ifndef TNFS_NET_SERVICE_H
#define TNFS_NET_SERVICE_H

#include "game/net/net_packet.h"
#include "game/net/net_session.h"

#include <stdint.h>

enum { NET_GAME_NAME = 43 };  /* "EAGAME." and the GUID's 35 hex digits and dots, a 0 */

/* 00477b78 (watcall (guid, out), from 00479338 and two more): 00473d43
 * (sprintf) of "EAGAME.%08x.%04x%04x.%02x%02x%02x%02x.%02x%02x%02x%02x"
 * (004b2764): the GUID's dword +0, words +4 and +6, bytes +8..+0xf. */
void net_game_name(const uint8_t *guid, char *out);

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    const NetEndOps *end;   /* its mutex and sessions (0052cdd0) too */
    const NetWaitOps *wait;
    int32_t (*tcp_kill)(void *ctx, NetSession *s);      /* 0052ce14 (cdecl) */
    void (*driver_close)(void *ctx, uint32_t handle);    /* the driver's +0x24 (eax the handle) */
    int32_t (*driver_stop)(void *ctx);                    /* the driver's +0x1c (eax the table) */
    void (*unservice)(void *ctx);                         /* 0049d0a0(00478db4) (cdecl): the session layer's service off */
} NetKillServiceOps;

/* 004790b0 killnetservice (cdecl (session), three callers):
 * 00478e18("killnetservice" 004b2998); not in 0052cdd0 (0049ca1c -1): 0.
 * +4 set: r = 0052ce14(session). Else 004795fc(session) when +0xc or +0x9c
 * is set; +0xa0 0; 00475c14(0) while +0xa4 (read again) is not 0; the
 * driver's +0x24 (+0x10), r = the driver's +0x1c. Then 0049c9d8(0052cdd0,
 * session), and 0049d0a0(00478db4) when 0049ca14(0052cdd0) is 0; r. */
int32_t net_kill_service(const NetKillServiceOps *ops, const NetCheck *c, NetSession *s);

enum { NET_GAME_PARAMS = 0x44, NET_MOST_PLAYERS = 32 };

extern const char NET_NEW_TWO[];   /* 004b29cc */
extern const char NET_NEW_MOST[];  /* 004b29f4 */

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    const NetEndOps *end;
    int32_t (*tcp_new)(void *ctx, NetSession *s, uint8_t *params, const char *password, int32_t arg);  /* 0052cdf4 (cdecl) */
    void (*message)(void *ctx, const char *format, int32_t value);   /* 00477a40 (cdecl) */
    NetServer *(*alloc)(void *ctx, const char *name, uint32_t size, uint32_t tag);  /* 00474cdc (cdecl) */
    void (*release)(void *ctx, NetServer *v);                         /* 00475260 (cdecl) */
    int32_t (*driver_open)(void *ctx, uint32_t handle, uint8_t *game);  /* the driver's +0x2c (eax, edx) */
    const int32_t *now;    /* 004c62a4 */
    const int32_t *stamp;  /* 004c62a8 */
    const uint32_t *tag;   /* 004c62b0 */
} NetNewGameOps;

/* 00479338 newnetgame (cdecl (session, params, password, arg), from
 * 00442970): 00478e18("newnetgame" 004b29c0); +4 set: 0052cdf4(session,
 * params, password, arg) returned. Params' +0x40 0: 32. +0x98 set:
 * 004795fc(session), +0x98 0. +0x40 (read again) below 2: 00477a40
 * (004b29cc); above 32: 00477a40(004b29f4, 32). +0xc = 00474cdc("netserv",
 * (+0x40 - 2) * 100 + 0xf8, 004c62b0); 0: 0. Else its +4 the session, +0x50
 * the params' 0x44 bytes, +8 0, +0x4c 0; +0x98 = 004c62a4, +0xa8 = arg,
 * +0x9c = 004c62a8, +0x14 = +0x28's 0x14 bytes; 00482890(server + 0x3c,
 * password, 16); 00477b78(+0x84, server + 0xc); the driver's +0x2c(+0x10,
 * server + 0xc) not 0: 1, else 00475260(server), +0xc 0, 0. */
int32_t net_new_game(const NetNewGameOps *ops, const NetCheck *c, NetSession *s, uint8_t *params,
                     const char *password, int32_t arg);

#endif
