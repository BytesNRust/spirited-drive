/* The network transport's game server side (net_server.h) */
#include "game/net/net_server.h"

int32_t net_bit_count(uint32_t bits)
{
    int32_t n = 0;
    for (; bits != 0; bits &= ~(0u - bits)) n++;
    return n;
}
