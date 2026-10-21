/* Port: PageUp / PageDown as menu keys. 00464bd0 (hot_key.c) names neither, so the exe's menus never see them; the
 * port's screens that turn pages ask for them (input_page_keys) and get their BIOS codes. */
#ifndef TNFS_PAGE_KEYS_H
#define TNFS_PAGE_KEYS_H

#include "game/sys/hot_key.h"

#include <stdint.h>

/* 0x4900 (PageUp) or 0x5100 (PageDown) when that key is held in the key flags, else 0. */
int32_t page_key(const uint8_t key_flags[KEY_FLAGS]);

#endif
