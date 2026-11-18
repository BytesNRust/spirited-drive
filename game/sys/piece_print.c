/* port (dep59 step 5, records by id + print): see piece_print.h. */
#include "game/sys/piece_print.h"

#include <string.h>

void piece_print_begin(PiecePrint *p, const char *domain)
{
    sha256_init(&p->sha);
    sha256_update(&p->sha, domain, strlen(domain) + 1);   /* with its NUL: "TNFS-TRK-1" can never run into the bytes after it */
}

void piece_print_add(PiecePrint *p, const void *data, size_t size)
{
    if (size > 0) sha256_update(&p->sha, data, size);
}

void piece_print_end(PiecePrint *p, uint8_t out[PIECE_PRINT_SIZE])
{
    sha256_digest(&p->sha, out);
    if (piece_print_is_none(out)) out[PIECE_PRINT_SIZE - 1] = 1;
}

int piece_print_is_none(const uint8_t print[PIECE_PRINT_SIZE])
{
    uint8_t any = 0;
    for (int i = 0; i < PIECE_PRINT_SIZE; i++) any |= print[i];
    return any == 0;
}

int piece_print_changed(const uint8_t before[PIECE_PRINT_SIZE], const uint8_t after[PIECE_PRINT_SIZE])
{
    return !piece_print_is_none(before) && !piece_print_is_none(after) && memcmp(before, after, PIECE_PRINT_SIZE) != 0;
}
