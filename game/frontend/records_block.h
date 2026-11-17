#ifndef RECORDS_BLOCK_H
#define RECORDS_BLOCK_H

#include "game/sys/track_registry.h"

#include <stdint.h>

/* The original's records live in one block per track, nine tracks. An added track (menu number 16..79) and a stray number
 * (-1, 10..15) have none: a screen reads an empty block, a write is refused. */
enum { RECORDS_BLOCK_TRACKS = 9, RECORDS_BLOCK_SIZE = 0xa6b };

static inline int records_block_known(int track) { return track >= 0 && track < RECORDS_BLOCK_TRACKS; }

/* the block of a COPY of the image (records_route_shown's, the check's scratch) that holds `track`'s records: its own; an added track\'s table is its
 * own (race_records_tracks.h) and the copy holds it in the first stage\'s block, an added track being a stage track (T7g); -1 for a number with none */
static inline int records_block_slot(int track) { return records_block_known(track) ? track : track_registry_is_added(track) ? 0 : -1; }

static inline const uint8_t *records_block_at(const uint8_t *image, int track)
{
    static const uint8_t empty[RECORDS_BLOCK_SIZE];
    const int slot = records_block_slot(track);
    return slot >= 0 ? image + RECORDS_BLOCK_SIZE * slot : empty;
}

#endif
