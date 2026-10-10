/* The race's horizon. Tracks pick one of two kinds in their INFO file
 * (track_info.h):
 *   - flat (ring 0): the "horz" bitmap of the FAM file, drawn as screen rows
 *     through a column table that bends it like a cylinder seen through the
 *     lens (0043a028, 0043ae64, 0043a18c);
 *   - ring (ring 1): a 3D ring of textured slices around the camera
 *     (sky_ring.h: 0043abfc, 0043b298, 0043b0c8).
 * Both sit on the line where a point far ahead at eye height projects
 * (0043af80). */
#ifndef TNFS_HORIZON_H
#define TNFS_HORIZON_H

#include "formats/fsh.h"
#include "game/race/track_draw.h"
#include "game/race/view.h"

#include <stdint.h>

enum {
    HORIZON_MAX_WIDTH = 640,
    HORIZON_ROWS = 0x1E0       /* 004fbb10..004fbcf0 */
};

typedef struct {
    int32_t width;                          /* 004fcd6c */
    int32_t scale;                          /* 004fcd70, screen rows per bitmap row, 16.16 */
    uint8_t row_map[HORIZON_ROWS];          /* 004fbb10 */
    int32_t columns[HORIZON_MAX_WIDTH + 1]; /* 004fbcf0, bitmap column per screen column */
} HorizonTables;

/* 00405154 -> 0043a028: for a viewport `width` pixels wide with a
 * horizontal field of view of `fov` degrees; viewport_scale is the
 * viewport's 16.16 vertical scale (+4 of the viewport struct, 0x10000). */
void horizon_tables(HorizonTables *t, int width, int fov, int32_t viewport_scale);

/* 0043af80: the projected horizon point (x, y), (INT32_MIN, INT32_MIN) if
 * behind the eye; the sky ring takes it as its third argument. */
void horizon_point(const View *view, int world_shift, int32_t yaw, const int32_t matrix[9], int32_t screen[2]);

/* 0043af80: screen y of the horizon for a camera with this yaw (24-bit)
 * and matrix; INT32_MIN if behind the eye. */
int32_t horizon_line(const View *view, int world_shift, int32_t yaw, const int32_t matrix[9]);

typedef struct {
    uint8_t *pixels;
    int pitch;
} HorizonTarget;

/* 0043ae64 + 0043a18c: the flat horizon. */
void horizon_draw_flat(const HorizonTables *t, const HorizonTarget *target, const ClipRect *clip, int left,
                       const FshEntry *horz, int32_t yaw, int mirrored, int32_t line);

#endif
