/* The network transport's packet layer (net_packet.h) */
#include "game/net/net_packet.h"

#include <string.h>

void net_checksum_put(NetChecksum sum, const uint8_t *data, uint8_t *out, uint32_t length, int32_t n)
{
    if (n <= 0) return;
    memset(out, 0, (size_t)n);
    uint32_t value = sum(data, length);
    for (int32_t i = n - 1; i >= 0; i--, value >>= 8) out[i] = (uint8_t)value;
}
