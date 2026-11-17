#ifndef RECORDS_BLOCK_H
#define RECORDS_BLOCK_H

#include <stdint.h>

/* The original's records live in one block per track, nine tracks. An added track (menu number 16..79) and a stray number
 * (-1, 10..15) have none: a screen reads an empty block, a write is refused. */
enum { RECORDS_BLOCK_TRACKS = 9, RECORDS_BLOCK_SIZE = 0xa6b };

static inline int records_block_known(int track) { return track >= 0 && track < RECORDS_BLOCK_TRACKS; }

static inline const uint8_t *records_block_at(const uint8_t *image, int track)
{
    static const uint8_t empty[RECORDS_BLOCK_SIZE];
    return records_block_known(track) ? image + RECORDS_BLOCK_SIZE * track : empty;
}

#endif
