/* Perspective projection of the 3D code (EA's 00475xxx routines).
 *
 * The focal length is half the viewport width times cot(fov / 2). Depth is
 * divided through a 1024-entry reciprocal table: z is normalised to a 10-bit
 * mantissa, the table holds focal / z in 16.16, and the product is shifted by
 * 16 plus the normalisation, rounding half up. Points at or behind the near
 * plane come back as (INT32_MIN, INT32_MIN). */
#ifndef TNFS_VIEW_H
#define TNFS_VIEW_H

#include <stdint.h>

enum { VIEW_RECIPROCALS = 1024 };

typedef struct {
    int32_t centre_x, centre_y;  /* 004c6234 / 004c6238 */
    int32_t focal_x, focal_y;    /* 004c623c / 004c6240 (negative) */
    int32_t near_z;              /* 004c6168 */
    int32_t recip[VIEW_RECIPROCALS];  /* table at *004c6220 */
} View;

/* 00475f68 + 00475fab + 00475e44: a viewport of width x height at (left,
 * top), with horizontal and vertical fields of view in degrees. */
void view_init(View *view, int left, int top, int width, int height, int fov_x, int fov_y);

/* 00475eac (and 0048cdd0 per point): camera space to screen. Returns 0 for a
 * point too close (and writes INT32_MIN twice, as the original). */
int view_project(const View *view, const int32_t camera[3], int32_t screen[2]);

#endif
