/* The menus' cheat codes: strings at 004baf44.. whose letters are stored
 * with 0x63 added (SKCART, EARALLYE, EXPLODE, CASINOVILLE, CONCEPTCAR,
 * TURBO, NITRO, LUNAR, ROCKYROAD), each with a position (a dword at
 * 004baf90..) into it. */
#ifndef TNFS_CHEAT_CODE_H
#define TNFS_CHEAT_CODE_H

#include <stdint.h>

enum { CHEAT_KEY_ADD = 0x63, CHEAT_KEY_FLAGS = 0x80 };

/* 00437258: the first of the keys 'A'..'Z' held in key_down (004c693c, by
 * key_code's scancode), or 0 when none, plus 0x63, against code[*pos]: on a
 * match *pos moves on, and 1 comes back when it reaches the code's NUL;
 * anything else puts *pos back to 0. Returns 0 but on the last letter. */
int cheat_code_step(int32_t *pos, const uint8_t *code, const uint8_t key_down[CHEAT_KEY_FLAGS]);

#endif
