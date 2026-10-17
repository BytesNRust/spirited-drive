/* The UDP driver (net_udp.h) as a network service's driver (port only, not
 * an exe function): its record for the drivers' table (0052cdc0) and the
 * handles the session keeps (+0x10, a uint32_t, so not a pointer) mapped to
 * the open drivers. The session's driver hooks call net_udp_* on
 * net_udp_service_get(handle). */
#ifndef TNFS_NET_UDP_SERVICE_H
#define TNFS_NET_UDP_SERVICE_H

#include "game/net/net_service.h"
#include "game/net/net_udp.h"

#include <stdint.h>

enum { NET_UDP_HANDLES = 4 };

extern const NetServiceDriver NET_UDP_DRIVER;  /* "UDP (LAN / IP)", its GUID ("TNFS UDP LAN/IP") for initnetservice */

typedef struct {
    uint16_t port;                        /* the one to bind (0: any) */
    const char *const *hosts;             /* direct hosts ("a.b.c.d:port"), NULL-terminated, or NULL */
    NetUdp *open[NET_UDP_HANDLES];        /* handle i + 1 */
} NetUdpService;

/* The driver's +0x20: a driver bound to the port with the direct hosts
 * added; its handle (1..NET_UDP_HANDLES), 0 when none could open */
uint32_t net_udp_service_open(NetUdpService *s);

/* The driver's +0x24: the handle's driver closed (0 or unknown: nothing) */
void net_udp_service_close(NetUdpService *s, uint32_t handle);

/* The handle's driver, NULL for 0 or one not open */
NetUdp *net_udp_service_get(const NetUdpService *s, uint32_t handle);

/* The hooks the protocol's ops take, ctx a NetUdpService (or a struct that
 * starts with one); a handle not open: 0 / nothing */
int32_t net_udp_service_send(void *ctx, uint32_t handle, uint8_t *address, int32_t message, uint8_t *data,
                             int32_t size);                                          /* +0x54 */
void net_udp_service_send_word(void *ctx, uint32_t handle, uint8_t *address, int32_t message, int32_t ecx,
                               int32_t arg);  /* +0x54 with no data (ack, bye, out: ecx and arg 0) */
int32_t net_udp_service_receive(void *ctx, uint32_t handle, uint8_t *address, int32_t *message, uint8_t *data,
                                int32_t size);                                       /* +0x58 */
int32_t net_udp_service_equal(void *ctx, uint32_t handle, const uint8_t *a, const uint8_t *b);  /* +0x40 */
int32_t net_udp_service_query(void *ctx, uint32_t handle);                           /* +0x50 */
int32_t net_udp_service_ready(void *ctx, uint32_t handle);                           /* +0x4c: 1 */
void net_udp_service_link(void *ctx, uint32_t handle, uint8_t *address, int32_t message, int32_t closing);  /* +0x5c */
int32_t net_udp_service_find(void *ctx, uint32_t handle, const char *name);          /* +0x34 */

#endif
