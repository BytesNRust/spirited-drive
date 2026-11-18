/* port (dep59 step 5, T3): the rules of a track piece, a mod of type=track (notes/dep59-step5-tracks-design.md), one for the game and the studio.
 * Only A-to-B tracks (kind=stage) in this version. A piece's files are named by its stem and a stage's number, as the CD's are (CY1, CY2, CY3):
 * stem TURINI with segments=2 has TURINI1 and TURINI2, each with SIMDATA/MISC/<R>.TRI, SIMDATA/MISC/<R>INFO and the day art
 * SIMDATA/ETRACKFM/<R>_001.FAM and SIMDATA/GTRACKFM/<R>_001.FAM (the language's folder: track_load.c). The dawn/dusk (_T01), mirror (_M01) and
 * rally (_R01) art of SIMDATA/NTRACKFM/ is optional. */
#ifndef TNFS_TRACK_PIECE_H
#define TNFS_TRACK_PIECE_H

#include "formats/exchange.h"
#include "formats/mod_ini.h"
#include "formats/validate.h"

#include <stdint.h>

enum {
    TRACK_PIECE_STEM_MAX = 7,       /* a root is the stem and a digit, 8 at most: DOS names, as the CD's */
    TRACK_PIECE_SEGMENTS_MAX = 3,   /* the CD's tracks have 3 stages */
    TRACK_PIECE_FILES = 4,          /* the TRI, its INFO and its two day FAMs, for each stage */
    TRACK_PIECE_PATH = 64
};

typedef enum { TRACK_KIND_STAGE = 0, TRACK_KIND_CIRCUIT = 1 } TrackKind;
typedef enum { TRACK_ROAD_ASPHALT = 0, TRACK_ROAD_DIRT = 1 } TrackRoad;
typedef enum {
    TRACK_EDGE_GRASS = 0, TRACK_EDGE_GRAVEL, TRACK_EDGE_SAND, TRACK_EDGE_SNOW, TRACK_EDGE_DIRT, TRACK_EDGE_WATER
} TrackEdge;
enum { TRACK_TIME_DAWN = 1, TRACK_TIME_DAY = 2, TRACK_TIME_DUSK = 4, TRACK_TIME_NIGHT = 8 };   /* times= as a set of bits */

/* what a good card says, in numbers: the defaults for a key not given are stage, 1 segment, asphalt, grass, day */
typedef struct {
    TrackKind kind;
    int segments;
    TrackRoad road;
    TrackEdge edge;
    int times;   /* TRACK_TIME_* */
} TrackPieceSpec;

/* 1 when `stem` may name a piece's files: 1..TRACK_PIECE_STEM_MAX of A-Z 0-9, and not beginning with AL, CL, CY or TR (the CD's roots:
 * race_sim_track_index still tells a root by its first two or three letters, so a piece that began like one would be taken for a factory track;
 * the race finds a piece's number by the whole stem, track_traits_race_set) */
int track_piece_stem_ok(const char *stem);

/* the card's keys into `out`; 1 when all are good. Findings go to `r` (PIECE.TYPE, PIECE.KIND, PIECE.SEGMENTS, PIECE.ROAD, PIECE.EDGE,
 * PIECE.TIMES, PIECE.STEM); kind=circuit is refused with "this version adds A-to-B tracks only" */
int track_piece_spec(const ModInfo *card, TrackPieceSpec *out, Report *r);

/* the files of stage `stage` (1..3) of the piece `stem`, as paths below the piece's root */
void track_piece_paths(const char *stem, int stage, char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH]);

/* 1 when `rel` (a cleaned path from the piece's root, any case) is one of the files a track piece of `stem` may carry besides its card: the
 * files of its stages 1..3, the optional art of NTRACKFM/<R>_T01|_M01|_R01.FAM, and the port's text files of track_light/ and track/ and
 * the PNGs of a folder of track_masks/ whose names begin with <R> (a root: the stem and 1..3) */
int track_piece_owns(const char *stem, const char *rel);

/* Everything the game asks of a track piece before it lets it race: the card (spec), each stage's files (PIECE.FILE_MISSING; found
 * whatever their case) and each one's own limits (validate_file). Findings go to `r`; 1 when none of them is fatal. `files` holds the
 * piece's files by their path below its root. `spec` may be NULL. */
int track_piece_check(const ModInfo *card, const FileSet *files, TrackPieceSpec *spec, Report *r);

/* the CRC-32 that decides how the piece races (a replay's check), never 0: the stages' TRI as on disk, in order, then kind, segments, road and
 * edge (one byte each). 0 when a stage's TRI is not in `files`. */
uint32_t track_piece_fingerprint(const TrackPieceSpec *spec, const char *stem, const FileSet *files);
/* the bytes track_piece_fingerprint is made of, in order, each stage's TRI then the four spec bytes, handed to `feed` one run at a time (a
 * print of another kind, the SHA-256 one, is made of the same stream); 0 when a stage's TRI is not in `files` (then `feed` got some of it) */
typedef void TrackPieceFeed(void *ctx, const void *data, size_t size);
int track_piece_stream(const TrackPieceSpec *spec, const char *stem, const FileSet *files, TrackPieceFeed *feed, void *ctx);

#endif
