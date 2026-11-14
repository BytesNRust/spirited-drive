/* A track's art file (.fam): the terrain, scenery and horizon bitmaps, and a circuit's animated ones */
#include "fuzz/fuzz.h"
#include "formats/chunk.h"
#include "game/race/track_art.h"

#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static TrackArt art;
    Chunk root;
    if (chunk_open(&root, data, size) != CHUNK_OK) return 0;
    memset(&art, 0, sizeof art);
    if (!track_art_load(&art, &root)) return 0;
    for (int s = 0; s < TRACK_TEXTURE_SLOTS; s++)
        for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++)
            if (art.slots[s][v].present) fuzz_touch(art.slots[s][v].bitmap.payload, art.slots[s][v].bitmap.payload_size);
    uint8_t tri_header[0x2c] = { 0 };   /* the TRI's short at +0x28 picks the animated slots */
    for (int id = 0; id < 4; id++) {
        tri_header[0x28] = (uint8_t)(id * 3);
        track_art_load_animated(&art, &root, tri_header);
    }
    return 0;
}
