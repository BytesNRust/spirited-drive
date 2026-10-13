#include "game/frontend/cheat_code.h"

#include "game/sys/key_codes.h"

/* 00437258 */
int cheat_code_step(int32_t *pos, const uint8_t *code, const uint8_t key_down[CHEAT_KEY_FLAGS])
{
    int key = 0;
    for (int k = 'A'; k <= 'Z'; k++)
        if (key_down[key_code((uint16_t)k)]) {
            key = k;
            break;
        }
    if (code[*pos] != key + CHEAT_KEY_ADD) {
        *pos = 0;
        return 0;
    }
    *pos += 1;
    return code[*pos] == 0;
}
