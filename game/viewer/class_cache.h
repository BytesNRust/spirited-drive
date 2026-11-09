/* port (frame budget): a row-by-row result kept between frames while its inputs have not changed. A stage that makes a row of output from the same
 * rows of up to CLASS_CACHE_INPUTS input planes (and some settings) asks class_cache_row_same for the row: 1 when the settings are those of the last
 * frame and every input row equals the one the row was last made from, so the stage's own output row (which it keeps) is still right; 0 when it must
 * make the row. Either way the cache then holds this frame's input rows. Every row must be asked each frame (the rows kept are those of the last whole frame).
 * Different rows may be asked from different threads (bands_rows). */
#ifndef TNFS_CLASS_CACHE_H
#define TNFS_CLASS_CACHE_H

#include <stddef.h>
#include <stdint.h>

enum { CLASS_CACHE_INPUTS = 6 };

typedef struct {
    uint8_t *copy[CLASS_CACHE_INPUTS];   /* the input planes as the rows were last made from them */
    size_t width, height;
    uint8_t *signature;                  /* the settings of the last frame */
    size_t signature_size;
    int ready;                           /* a frame has been taken whole under `signature` */
    int reuse;                           /* this frame has the last frame's settings and size: its rows may be the same */
} ClassCache;

/* A new frame: the settings (`signature`, `signature_size` bytes, padding zeroed by the caller), the planes' size and which planes are asked about (bit i for
 * input i). Any change in them makes every row be made. Run on one thread, before the rows are asked. */
void class_cache_begin(ClassCache *cache, const void *signature, size_t signature_size, size_t width, size_t height, unsigned used);

/* Row y of the planes `input` (a NULL plane is not compared; its presence belongs in the signature). */
int class_cache_row_same(ClassCache *cache, int y, const uint8_t *const input[CLASS_CACHE_INPUTS]);

#endif
