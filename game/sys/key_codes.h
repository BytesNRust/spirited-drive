/* The game's key names: a configured key (a character, or a BIOS code with
 * the scancode in the high byte) to the set-1 scancode its key flags
 * (004c693c) are indexed by. */
#ifndef TNFS_KEY_CODES_H
#define TNFS_KEY_CODES_H

#include <stdint.h>

/* 00487b84 (cdecl key): with a high byte, (high | 0x80) is looked for, else
 * the low byte, in the first match of the five 0x5b-byte rows at 004c69bc
 * (plain, shift and three more, as 00476ef0 picks them); found at i, the
 * result is ((i + 1) mod 0x5b) - 1 (the last column gives -1); 0 when
 * none matches. */
int32_t key_code(uint16_t key);

/* 00426b28 (watcall key), german the word 0052b31c: 0, the key as it is;
 * else 0x21..0x40, 0x5b..0x60, 0x7b..0x9a and 0xe1 through the German
 * layout's table (0 where it has none), Y and Z swapped, the rest as it is. */
int32_t key_layout(int32_t key, int32_t german);

#endif
