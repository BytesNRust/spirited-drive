/* See crc32.h. */
#include "formats/crc32.h"

uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    static uint32_t table[256];
    if (table[1] == 0) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    }
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
