/* The network transport's services (EA's library): a session record's
 * life in the sessions' list (0052cdd0); 004790b0 ends one. */
#ifndef TNFS_NET_SERVICE_H
#define TNFS_NET_SERVICE_H

#include "game/net/net_packet.h"
#include "game/net/net_session.h"

#include <stdint.h>

enum { NET_GUID = 16 };

/* 00477bcc (watcall (a, b), from 00478f0c): two GUIDs the same? The dword
 * +0, the words +4 and +6, the bytes +8..+0xf; 1 / 0. */
int32_t net_guid_equal(const uint8_t *a, const uint8_t *b);

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

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    int32_t (*tcp_shut)(void *ctx, NetSession *s, int32_t release);   /* 0052ce08 (cdecl) */
    void (*driver_leave)(void *ctx, uint32_t handle, uint8_t *game);  /* the driver's +0x30 (eax, edx) */
    void (*release)(void *ctx, NetServer *v);                         /* 00475260 (cdecl) */
} NetShutGameOps;

/* 004794b4 shutnetgame (cdecl (session, release), from 00442970):
 * 00478e18("shutnetgame" 004b2a28); +4 set: 0052ce08(session, release)
 * returned. No server (+0xc): 0. Its +8 0: the driver's +0x30(+0x10, server
 * + 0xc), then (+0xc read again) its +0xc byte 0 and +8 1. Release not 0:
 * 00475260(server), +0xc 0. 1. */
int32_t net_shut_game(const NetShutGameOps *ops, const NetCheck *c, NetSession *s, int32_t release);

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    int32_t (*tcp_open)(void *ctx, NetSession *s);                     /* 0052ce04 (cdecl) */
    int32_t (*driver_open)(void *ctx, uint32_t handle, uint8_t *game);  /* the driver's +0x2c (eax, edx) */
    const int32_t *now;  /* 004c62a4 */
} NetOpenGameOps;

/* 00479534 opennetgame (cdecl (session), no direct callers): 00478e18
 * ("opennetgame" 004b2a34); +4 set: 0052ce04(session) returned. No server
 * (+0xc) or its +8 0: 0. Else +8 1, 00477b78(+0x84, server + 0xc), the
 * driver's +0x2c(+0x10, server + 0xc) 0: +8 0, 0. Else +0x9c 1, each of the
 * server's +0x90 (its params' most players, read again) entries' +0x5c =
 * 004c62a4 (read after +0x2c); 1. */
int32_t net_open_game(const NetOpenGameOps *ops, const NetCheck *c, NetSession *s);

/* A network service's driver (the tables at 0052cdc0, 004c62a0 of them):
 * +0 its id, +4 its name (inline); its methods' table is the driver */
typedef struct {
    int32_t id;           /* +0 */
    const uint8_t *name;  /* +4: inline */
} NetServiceDriver;

/* getnetservices' callback (cdecl (name, id, a, b)): 0 stops it */
typedef struct {
    void *ctx;
    int32_t (*fn)(void *ctx, const uint8_t *name, int32_t id, int32_t a, int32_t b);
} NetServicesCallback;

enum { NET_SERVICES_MOST = 32 };

typedef struct {
    void *ctx;
    const NetCheckOps *check;
    NetServiceDriver *const *drivers;  /* 0052cdc0 */
    const int32_t *count;              /* 004c62a0 */
    /* the driver's +0x14 (eax &a, edx &b): not 0, the driver there */
    int32_t (*driver_info)(void *ctx, NetServiceDriver *d, int32_t *a, int32_t *b);
    /* 0052ce20 (cdecl, getnetservices' four), NULL when not set */
    int32_t (*tcp_services)(void *ctx, const NetServicesCallback *cb, int32_t arg2, int32_t arg3, uint32_t most);
} NetServicesOps;

/* 00478e6c getnetservices (cdecl (callback, arg2, arg3, most), from
 * 004432d4): 00478e18("getnetservices" 004b2978). most at most
 * 32 (unsigned): for each of the 004c62a0 (read again, signed) drivers at
 * 0052cdc0 whose +0x14(&a, &b) is not 0, callback(driver + 4, *driver, a, b);
 * 0 from it: 0. Then 0052ce20 set: 0052ce20(callback, arg2, arg3, most)
 * returned, else 1. */
int32_t net_get_services(const NetServicesOps *ops, const NetCheck *c, const NetServicesCallback *cb, int32_t arg2,
                         int32_t arg3, uint32_t most);

#endif
