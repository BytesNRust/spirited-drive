/* The port's PageUp / PageDown (page_keys.c): named from the key flags by their set-1 scancodes, 0 for any other. */
#include "sys/hot_key.h"
#include "sys/page_keys.h"
#include "test.h"

#include <string.h>

enum { SC_PAGE_UP = 0x49, SC_PAGE_DOWN = 0x51, SC_UP = 0x48 };

int main(void)
{
    uint8_t flags[KEY_FLAGS];
    memset(flags, 0, sizeof flags);
    CHECK_EQ_INT(page_key(flags), 0);
    flags[SC_PAGE_DOWN] = 1;
    CHECK_EQ_INT(page_key(flags), 0x5100);
    flags[SC_PAGE_DOWN] = 0;
    flags[SC_PAGE_UP] = 1;
    CHECK_EQ_INT(page_key(flags), 0x4900);
    memset(flags, 0, sizeof flags);
    flags[SC_UP] = 1;
    CHECK_EQ_INT(page_key(flags), 0);
    return TEST_REPORT();
}
