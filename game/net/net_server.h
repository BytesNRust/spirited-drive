/* The network transport's game server side (EA's library): 00478030
 * answers the requests that reach the server and the pieces it uses. */
#ifndef TNFS_NET_SERVER_H
#define TNFS_NET_SERVER_H

#include <stdint.h>

/* 00477c6c (watcall (bits), from 00478030): how many bits are set (the
 * lowest cleared, x & ~-x, until none). */
int32_t net_bit_count(uint32_t bits);

#endif
