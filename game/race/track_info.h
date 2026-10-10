/* Per-track sky settings (0043a370): simdata/misc/<TRACK>INFO, a text file
 * of values each preceded by "#" comment lines, in this order:
 *
 *   vert offset near/far, zoffset near/far, height near/far, radius
 *   near/far (decimals, kept 16.16), colour top and bottom ("r g b", 5 bits
 *   each, packed r<<10 | g<<5 | b), ring type (1: a 3D sky ring, else a flat
 *   horizon bitmap), ring y offset, ring height, then three groups of five
 *   time-of-day values (the fifth of each << 8) and a final flag that also
 *   turns the ring on.
 *
 * The mirrored and reversed track variants use built-in tables instead
 * (004bb094), not ported yet: TODO(0043a370). */
#ifndef TNFS_TRACK_INFO_H
#define TNFS_TRACK_INFO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t vert_offset_near, vert_offset_far;  /* 004fccf4, 004fccf8 */
    int32_t z_offset_near, z_offset_far;        /* 004fccfc, 004fcd00 */
    int32_t height_near, height_far;            /* 004fcd08, 004fcd10 */
    int32_t radius_near, radius_far;            /* 004fcd04, 004fcd0c */
    uint16_t colour_top, colour_bottom;         /* 004fcd14, 004fcd18 */
    int32_t ring;                               /* 004fcd1c */
    int32_t ring_y_offset;                      /* 004fcd20 */
    int32_t ring_height;                        /* 004fcd24 */
    int32_t time_of_day[3][5];                  /* 004fcd28.. (meaning unknown) */
    int32_t final_flag;                         /* 004fcd64 */
} TrackInfo;

/* 00431f38: a decimal as 16.16 ("-45" or "0.25"). */
int32_t track_info_decimal(const char *text);

/* Parses the whole file; returns 0 if it ends early. The hi-res scaling of
 * the ring values (only for 320 x 200, 0043a370) is not applied. */
int track_info_parse(TrackInfo *info, const char *text, size_t size);

#endif
