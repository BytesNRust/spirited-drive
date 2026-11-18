/* port (dep59 step 5, records by id + print): the print of a track or car piece, the SHA-256 of what decides how it races or drives
 * (the stream track_piece_stream and car_pieces make), behind the name of its domain so a track's and a car's never meet. 32 bytes; all
 * zeros is "no print" (the stock tracks and cars, and a piece made by hand in a test). The CRC-32 the pieces also keep (their
 * `fingerprint`) is what Replay.rpx files made before this read; a record or a new Replay.rpx holds this one. */
#ifndef TNFS_PIECE_PRINT_H
#define TNFS_PIECE_PRINT_H

#include "game/sys/sha256.h"

#include <stddef.h>
#include <stdint.h>

enum { PIECE_PRINT_SIZE = 32 };

#define PIECE_PRINT_TRACK "TNFS-TRK-1"
#define PIECE_PRINT_CAR "TNFS-CAR-1"

typedef struct {
    Sha256 sha;
} PiecePrint;

void piece_print_begin(PiecePrint *p, const char *domain);
void piece_print_add(PiecePrint *p, const void *data, size_t size);
/* the print into `out`; the state is spent. Never all zeros (the one stream in 2^256 that would be is 1 in its last byte) */
void piece_print_end(PiecePrint *p, uint8_t out[PIECE_PRINT_SIZE]);
/* 1 when `print` is all zeros: no print */
int piece_print_is_none(const uint8_t print[PIECE_PRINT_SIZE]);

/* 1 when a piece that had the print `before` has the print `after` and they differ: its rules changed and its records start anew (the old ones
 * are kept). 0 when either is none: a piece not there, or one that has no print, has no records by print to part from */
int piece_print_changed(const uint8_t before[PIECE_PRINT_SIZE], const uint8_t after[PIECE_PRINT_SIZE]);

#endif
