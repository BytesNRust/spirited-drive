/* The port's PageUp / PageDown menu keys (page_keys.h). */
#include "game/sys/page_keys.h"

enum { SC_PAGE_UP = 0x49, SC_PAGE_DOWN = 0x51 };

int32_t page_key(const uint8_t key_flags[KEY_FLAGS])
{
    if (key_flags[SC_PAGE_DOWN] != 0) return 0x5100;
    if (key_flags[SC_PAGE_UP] != 0) return 0x4900;
    return 0;
}
