/* The port's network driver: UDP over the LAN and the internet, in place of
 * the exe's IPX / DirectPlay / modem drivers (port only, not an exe
 * function). Each call does what the IPX driver's slot does (its table at
 * 004c64a0, 004815b8..00481e0c),
 * so the session's driver ops can call it as they are. */
#ifndef TNFS_NET_UDP_H
#define TNFS_NET_UDP_H

#include <stdint.h>

enum {
    NET_UDP_ADDRESS = 0x14,  /* the address the protocol copies: IPv4, port (network order), zeros */
    NET_UDP_MOST = 0x118,    /* a datagram's data at most (00481ad4) */
    NET_UDP_HOSTS = 8,       /* direct hosts find asks besides the LAN */
    NET_UDP_QUEUE = 64       /* datagrams held (the IPX driver's 64 buffers) */
};

typedef struct NetUdp NetUdp;

/* +0x18: the largest datagram (message and data); 0 without sockets */
int32_t net_udp_present(void);

/* +0x20: a socket bound to port (0: any), broadcast on, non-blocking; NULL
 * on failure */
NetUdp *net_udp_open(uint16_t port);

/* +0x24: the socket closed, the driver freed (NULL: nothing) */
void net_udp_close(NetUdp *u);

/* The port the socket is bound to */
uint16_t net_udp_port(const NetUdp *u);

/* A direct host ("a.b.c.d:port") that find and broadcasts also reach: 1, or
 * 0 for a bad one or no room */
int32_t net_udp_add_host(NetUdp *u, const char *host);

/* +0x34: NET_SERVER_ASK with the name (0x20 bytes) to the LAN's broadcast
 * and each direct host; 1 */
int32_t net_udp_find(NetUdp *u, const char *name);

/* +0x40: the same IPv4 and port */
int32_t net_udp_equal(const uint8_t *a, const uint8_t *b);

/* +0x44: its own address (the LAN's, else the loopback; its port); 1 */
int32_t net_udp_self(const NetUdp *u, uint8_t *out);

/* +0x50: a datagram held or waiting: 1, else 0 */
int32_t net_udp_query(NetUdp *u);

/* +0x54: message (4 bytes big-endian) then size bytes of data (cut at
 * NET_UDP_MOST) to the address, or NULL for the broadcast (the LAN and the
 * direct hosts); 1 when all of it went */
int32_t net_udp_send(NetUdp *u, const uint8_t *address, int32_t message, const uint8_t *data, int32_t size);

/* +0x58: the first datagram held: its address, message and data (size at
 * most), the data's length; none: message 0, 0 */
int32_t net_udp_receive(NetUdp *u, uint8_t *address, int32_t *message, uint8_t *data, int32_t size);

/* +0x5c: the datagrams held from the address with the message dropped; 1 */
int32_t net_udp_link(NetUdp *u, const uint8_t *address, int32_t message, int32_t closing);

#endif
