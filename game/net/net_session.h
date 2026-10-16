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

/* 00479ad4 (cdecl (session, a, b), from 00477e94, 00478030, 00479250,
 * 00479770 and 004799a8): two network addresses the same? With +4 set the
 * TCP layer's 0052ce10 (session, a, b), else the driver's +0x40 (eax the
 * handle +0x10, edx a, ebx b); its eax returned. */
int32_t net_address_equal(const NetEqualOps *ops, NetSession *s, const uint8_t *a, const uint8_t *b);

#endif
