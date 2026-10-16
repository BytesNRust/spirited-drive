/* The network transport's session (EA's library, 00477e94..0047bfff): a
 * session record (0050a260 for the race's) runs either the TCP layer's
 * hooks (0052cdec..0052ce20, set by 0047fab0) or its driver's methods (the
 * table at +8, eax the driver's handle +0x10). */
#ifndef TNFS_NET_SESSION_H
#define TNFS_NET_SESSION_H

#include <stdint.h>

/* The session record's words read here; read again after every call */
typedef struct {
    int32_t tcp;          /* +4: not 0, the TCP layer's hooks instead of the driver's methods */
    uint32_t handle;      /* +0x10: the driver's, its methods' eax */
} NetSession;

typedef struct {
    void *ctx;
    int32_t (*tcp_equal)(void *ctx, NetSession *s, const uint8_t *a, const uint8_t *b);  /* 0052ce10 (cdecl; 0048102c) */
    int32_t (*driver_equal)(void *ctx, uint32_t handle, const uint8_t *a, const uint8_t *b);  /* the driver's +0x40 */
} NetEqualOps;

enum { NET_PLAYERS = 31, NET_PLAYER_SIZE = 100, NET_PLAYER_SEEN = 0x5c };

/* The server's record (the session's +0xc); read again after every call */
typedef struct {
    NetSession *session;  /* +4 */
    uint32_t players;     /* +0x4c: a bit for each entry in use */
    uint8_t *entries;     /* +0x94: NET_PLAYERS entries of NET_PLAYER_SIZE bytes, each the player's address first */
} NetServer;

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

#endif
