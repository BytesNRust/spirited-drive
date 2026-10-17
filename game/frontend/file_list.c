#include "game/frontend/file_list.h"

#include <stddef.h>

int file_list_same_name(const uint8_t *a, const char *b)
{
    for (;; a++, b++) {
        const int x = *a >= 'A' && *a <= 'Z' ? *a + 0x20 : *a;
        const int y = (uint8_t)*b >= 'A' && (uint8_t)*b <= 'Z' ? (uint8_t)*b + 0x20 : (uint8_t)*b;
        if (x != y) return 0;
        if (y == 0) return 1;
    }
}

/* 0042ad94 */
void file_list_drop_replays(uint8_t *names, int16_t *count)
{
    /* 004aeff8, 004af004, 004af010, 004af01c */
    static const char *const REPLAY[] = { "REPLAY.RPL", "REPLAY1.RPL", "REPLAY2.RPL", "REPLAY3.RPL" };
    for (int i = 0; i < *count; i++) {
        uint8_t *name = names + FILE_LIST_NAME * (size_t)i;
        int drop = 0;
        for (size_t r = 0; r < sizeof REPLAY / sizeof REPLAY[0]; r++)
            if (file_list_same_name(name, REPLAY[r])) drop = 1;
        if (!drop) continue;
        for (int j = i; j < *count; j++) {
            uint8_t *to = names + FILE_LIST_NAME * (size_t)j;
            const uint8_t *from = to + FILE_LIST_NAME;
            while ((*to++ = *from++) != 0) {}
        }
        i--;
        *count = (int16_t)(*count - 1);
    }
}
