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

#endif
