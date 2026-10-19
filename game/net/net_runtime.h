/* The network transport as the game runs it (port only, not an exe
 * function): one instance of EA's library globals (the sessions' list
 * 0052cdd0, the packet connections' lists 0052ce40 / 0052ce24, the counters
 * at 004c62a0..004c6300) over the UDP driver, and the library's calls with
 * their ops filled in.
 *
 * The original ran 00478db4 (the sessions) and 0047aaf4 (the packet
 * connections) from a service thread (0049ceb8, registered through
 * 0049d028). The port runs them on the main thread instead, from
 * net_runtime_service, once for each tick of the 100 Hz clock that passed:
 * the library's waits (0047337d, 0047ac18, 004790b0) call the yield hook,
 * whose default is net_runtime_service. No threads, so the mutexes are no-ops. */
#ifndef TNFS_NET_RUNTIME_H
#define TNFS_NET_RUNTIME_H

#include "game/net/net_link.h"
#include "game/net/net_packet.h"
#include "game/net/net_request.h"
#include "game/net/net_server.h"
#include "game/net/net_service.h"
#include "game/net/net_udp_service.h"

#include <stddef.h>
#include <stdint.h>

/* A session record and the inline blocks the original keeps inside it
 * (+0x14, +0x28, +0x3c, +0x70, +0x84), in its order: the join record
 * (NET_RECORD bytes from self) carries text_3c at its +0x14 and text_70 at
 * its +0x48 */
typedef struct {
    NetSession s;
    uint8_t address[NET_ADDRESS], self[NET_ADDRESS];
    char text_3c[NET_TEXT_3C], text_70[NET_TEXT_70];
    uint8_t guid[NET_GUID];
} NetSessionRecord;
typedef char net_session_record_check[offsetof(NetSessionRecord, text_3c) - offsetof(NetSessionRecord, self) == 0x14 &&
                                              offsetof(NetSessionRecord, text_70) - offsetof(NetSessionRecord, self) == 0x48 &&
                                              offsetof(NetSessionRecord, guid) - offsetof(NetSessionRecord, self) == NET_RECORD
                                          ? 1 : -1];

/* The game server's record (00474cdc's "netserv") with its inline blocks */
typedef struct {
    NetServer v;
    uint8_t game[0x30];
    char password[NET_PASSWORD];
    uint8_t params[NET_GAME_PARAMS];
    uint8_t entries[NET_PLAYERS * NET_PLAYER_SIZE];
} NetServerRecord;

typedef struct NetRuntime NetRuntime;

struct NetRuntime {
    NetUdpService udp;
    NetServiceDriver *drivers[1];          /* 0052cdc0 */
    int32_t driver_count;                  /* 004c62a0 */
    NetQueue sessions, open, closed;       /* 0052cdd0, 0052ce40, 0052ce24 */
    void *lock;                            /* 004c62c4 */
    int32_t init_started, packet_started;  /* 004c62f0, 004c62f4 */
    int32_t service_started, packets_on;   /* 00478db4 / 0047aaf4 on the service list */
    int32_t now, stamp, resend, trace_level, next_request, deadline;  /* 004c62a4..004c62c0 */
    uint32_t tag;                          /* 004c62b0 */
    int32_t timeout;                       /* 004c6170 */
    uint32_t room;                         /* 004c62e4 */
    int32_t next_conn, packet_trace, open_state;  /* 004c62f8, 004c62fc, 004c6300 */
    int32_t surface_locked;                /* 004c7ac0 */
    int32_t clock;                         /* 0052eebc as the library reads it */
    uint32_t serviced;                     /* the last clock tick serviced */
    int32_t serving;                       /* inside net_runtime_service */
    NetCheck check;

    /* the game's hooks */
    void *ctx;
    uint32_t (*ticks)(void *ctx);          /* the 100 Hz clock (timer_ticks) */
    void (*yield)(void *ctx);              /* 00475c14(0): NULL runs net_runtime_service */
    /* the session's +0xa8 (newnetgame's fourth argument): 0 turns a joining player away */
    int32_t (*accept)(void *ctx, int32_t fn, NetSession *s, uint8_t *record);
    void (*message)(void *ctx, const char *text);  /* 00477a40, NULL: stderr */

    /* the ops, filled by net_runtime_init and pointing into this record */
    NetMutexOps mutex;
    NetCheckOps check_ops;
    NetEqualOps equal;
    NetClockOps clock_ops;
    NetWaitOps wait;
    NetEndOps end;
    NetKillServiceOps kill_service;
    NetInitServiceOps init_service;
    NetServicesOps services;
    NetNewGameOps new_game;
    NetShutGameOps shut_game;
    NetKillOps kill;
    NetRequestOps request;
    NetSetOps set_game;
    NetPlayersOps players;
    NetGamesOps games;
    NetGameInfoOps info;
    NetAddPlayerOps add;
    NetServeOps serve;
    NetReceiveOps receive;
    NetPollOps poll;
    NetSessionsOps sessions_ops;
    NetLinkOps link;
    NetDriver link_driver;                 /* 004c62c8 */
    NetPacketOps packet;
    NetQueueMessage queue_message;
    NetCloseOps close;
    NetPacketRecvOps recv;
    NetShutdownOps shutdown;
    NetStartOps start;
    NetOpenOps open_ops;
    NetConnectOps connect;
};

/* The library's globals as the exe starts with them, the UDP driver bound to
 * port (0: any) with the direct hosts ("a.b.c.d:port", NULL-terminated, or
 * NULL), the ops filled in. ticks is required. */
void net_runtime_init(NetRuntime *rt, uint16_t port, const char *const *hosts, void *ctx, uint32_t (*ticks)(void *ctx));

/* The service thread's turn: for each clock tick passed since the last,
 * 00478db4 then 0047aaf4 (at most a second's worth; not re-entered) */
void net_runtime_service(NetRuntime *rt);

/* The library's calls (EA's names) */
/* initnetservice on the UDP driver: the game's GUID (the game's name made
 * from it) and the two texts; r's pointers wired to its blocks */
int32_t net_runtime_init_service(NetRuntime *rt, NetSessionRecord *r, const uint8_t *guid, const char *text_3c,
                                 const char *text_70);
int32_t net_runtime_get_services(NetRuntime *rt, const NetServicesCallback *cb);  /* getnetservices(cb, 0, 0, 32) */
int32_t net_runtime_kill_service(NetRuntime *rt, NetSession *s);        /* killnetservice */
int32_t net_runtime_new_game(NetRuntime *rt, NetSession *s, uint8_t *params, const char *password, int32_t arg);
int32_t net_runtime_end_game(NetRuntime *rt, NetSession *s);            /* endnetgame */
int32_t net_runtime_shut_game(NetRuntime *rt, NetSession *s, int32_t release);  /* shutnetgame */
int32_t net_runtime_get_games(NetRuntime *rt, NetSession *s, const NetGamesCallback *cb, int32_t timeout);
int32_t net_runtime_set_game(NetRuntime *rt, NetSession *s, const uint8_t *address, const char *password);
int32_t net_runtime_get_players(NetRuntime *rt, NetSession *s, const uint8_t *address, const NetPlayerCallback *cb);
int32_t net_runtime_kill_player(NetRuntime *rt, NetSession *s, const uint8_t *address);
/* opennetconnection: h's session (+0x24) and peer address (+0x28) set */
int32_t net_runtime_open(NetRuntime *rt, NetPacketHandle *h, uint32_t tag, int32_t crc32, uint32_t size, uint32_t bits);
int32_t net_runtime_close(NetRuntime *rt, NetPacketHandle *h);         /* closepacketconnection */
int32_t net_runtime_send(NetRuntime *rt, NetPacketHandle *h, const uint8_t *data, uint32_t n);  /* sendpacket */
void net_runtime_shutdown(NetRuntime *rt);                              /* 0047ab80 */

#endif
