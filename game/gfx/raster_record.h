/* port (render bands P3): a target that is being recorded draws nothing at once. Each
 * raster primitive (quad.c, quad_tri.c, flat.c, sprite_scaled.c, modern_road.c) called on the pixels the active recorder
 * was begun for copies its arguments into a command instead; a flush then replays the commands per band of rows on several threads, in the order
 * they were recorded. The bands partition the pixels and each command paints inside a band what it paints there whole
 * (tests/test_band_exact.c), so the picture is byte for byte the direct one. With no recorder active (TNFS_BANDS=0, or
 * any other pixels) the primitives are the plain direct path. */
#ifndef TNFS_RASTER_RECORD_H
#define TNFS_RASTER_RECORD_H

#include "formats/fsh.h"
#include "game/gfx/quad.h"

#include <stdint.h>

enum {
    RASTER_RECORD_MAX = 8192,   /* a full list is flushed before the next command is added */
    RASTER_CMD_INTS = 32        /* uv, screen / points, camera-space corners: most any primitive's; a lit road cell's darkness, heat and warm-table pointer after them */
};

typedef struct RasterCmd RasterCmd;

/* Draws `c` into rows [top, bottom) of `whole` (the target as the primitive was called with, no recorder): either
 * through a copy narrowed to those rows, for the primitives whose rows are independent, or on `whole` with the rows as a
 * store window, for the ones whose edges or retrace a narrowed clip would move (quad.c, flat.c). */
typedef void (*RasterReplay)(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom);

struct RasterCmd {
    RasterReplay replay;
    uint8_t *pixels;                      /* the target it was called with */
    int pitch, clip_left, clip_top, clip_right, clip_bottom;
    int32_t top, bottom;                  /* the rows it can touch, [top, bottom) */
    const FshEntry *texture;
    const uint8_t *shape;                 /* a sprite's shape, or a shade table */
    const void *table;                    /* shade, remap, blend rows or view */
    int32_t mode, colour, count, a, b, c, d;
    int32_t v[RASTER_CMD_INTS];
};

struct RasterRecorder {
    const uint8_t *pixels;                /* the buffer it records for */
    int count;
    int32_t top, bottom;                  /* the rows the recorded targets' clips span */
    RasterCmd cmds[RASTER_RECORD_MAX];
};

/* The active recorder, or NULL. Set on the main thread by raster_record_begin and cleared by raster_record_end, and
 * cleared too for the length of a flush, so the replays on the band threads draw. */
extern struct RasterRecorder *raster_active;

/* The recorder that takes the draws to `t`, or NULL for the direct path. */
static inline struct RasterRecorder *raster_recorder_for(const QuadTarget *t)
{
    return raster_active != NULL && t != NULL && t->pixels == raster_active->pixels ? raster_active : NULL;
}

/* Empties the recorder and makes it the active one for draws to `pixels`. */
void raster_record_begin(struct RasterRecorder *r, const uint8_t *pixels);

/* Replays what is recorded and deactivates the recorder. */
void raster_record_end(struct RasterRecorder *r);

/* The next command, for target `t` (its pixels and clip kept) replayed by `replay`; rows from the primitive's
 * geometry through raster_cmd_rows, or every row until it says. Flushes first when full. */
RasterCmd *raster_record_add(struct RasterRecorder *r, const QuadTarget *t, RasterReplay replay);

/* The rows the points span (a pixel and a rounding either side), (x, y) pairs. */
void raster_cmd_rows(RasterCmd *c, const int32_t *xy, int points);

/* The target for a replay: c's, without the recorder. */
QuadTarget raster_cmd_target(const RasterCmd *c);

/* A copy of `whole` narrowed to rows [top, bottom) of its clip; 0 when nothing is left. */
int raster_narrow(const QuadTarget *whole, int32_t top, int32_t bottom, QuadTarget *out);

/* Replays what touches [top, bottom), in recorded order. */
void raster_record_replay(const struct RasterRecorder *r, int32_t top, int32_t bottom);

/* The runner a flush hands the bands to: fn(ctx, top, bottom) over [top, bottom) split however it likes (bands_rows);
 * NULL, the default, replays whole on the caller. Kept apart so the gfx code does not depend on the platform's threads. */
typedef void (*RasterRunner)(int top, int bottom, void (*fn)(void *ctx, int top, int bottom), void *ctx);
void raster_record_set_runner(RasterRunner runner);

/* Replays everything through the runner (one plain replay for a short list) and empties the recorder. */
void raster_record_flush(struct RasterRecorder *r);

/* Primitives that are not recorded call this first: what has been recorded for `t` is drawn before they draw. */
void raster_record_sync(const QuadTarget *t);

#endif
