/* Per-track sky settings (0043a370): simdata/misc/<TRACK>INFO, a text file
 * of values each preceded by "#" comment lines, in this order:
 *
 *   vert offset near/far, zoffset near/far, height near/far, radius
 *   near/far (decimals, kept 16.16), colour top and bottom ("r g b", 5 bits
 *   each, packed r<<10 | g<<5 | b), ring type (1: a 3D sky ring, else a flat
 *   horizon bitmap), ring y offset, ring height, then three groups of five
 *   time-of-day values (the fifth of each << 8) and a final flag that also
 *   turns the ring on. Without the file (or for the time of day 1) built-in
 *   rows by the track's stem (004bb094). */
#ifndef TNFS_TRACK_INFO_H
#define TNFS_TRACK_INFO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t vert_offset_near, vert_offset_far;  /* 004fccf4, 004fccf8 */
    int32_t z_offset_near, z_offset_far;        /* 004fccfc, 004fcd00 */
    int32_t height_near, height_far;            /* 004fcd08, 004fcd10 */
    int32_t radius_near, radius_far;            /* 004fcd04, 004fcd0c */
    int32_t colour_top, colour_bottom;          /* 004fcd14, 004fcd18 */
    int32_t ring;                               /* 004fcd1c */
    int32_t ring_y_offset;                      /* 004fcd20 */
    int32_t ring_height;                        /* 004fcd24 */
    /* 004fcd28..: [value][group], three words a row: each group's red,
     * green, blue, saturation and brightness (<< 8) for 004471ac */
    int32_t time_of_day[5][3];
    int32_t final_flag;                         /* 004fcd64 */
    int32_t sky_flag;                           /* 004fcd74: final_flag, 0 for time of day 1 off the 't' tracks */
    int32_t unknown_4fccf0;                     /* 004fccf0: 0 after the load */
} TrackInfo;

enum { TRACK_INFO_LINE = 0x7f, TRACK_INFO_ROWS = 16, TRACK_INFO_ROW_WORDS = 33 };

/* 0043a370's calls: the file (00429010 fopen "r", 004822ba fgets, 00429018
 * fclose) and the tint (004471ac). */
typedef struct {
    void *ctx;
    int32_t (*open)(void *ctx, const char *name);   /* a handle, 0 for none */
    /* fgets: at most size - 1 characters up to a newline; 0 at the end, the line left as it was */
    int (*gets)(void *ctx, char *line, int32_t size, int32_t file);
    void (*close)(void *ctx, int32_t file);
    void (*tint)(void *ctx, int32_t red, int32_t green, int32_t blue, int32_t saturation, int32_t brightness);
} TrackInfoOps;

typedef struct {
    const char *stem;           /* 004fb7c6: the track's */
    const char *misc_dir;       /* 005246e8: paths slot 17 */
    int32_t use_file;           /* 004bb090: 1 as the exe starts, never written */
    int32_t time_of_day;        /* 004fa0ec: 0..2 */
    int32_t screen_mode;        /* 00512018: 0 scales the ring's offset and height by 200 / 480 */
} TrackInfoInputs;

/* 0043a370 (watcall stem). The name "%s%sInfo" (misc dir, stem) opened;
 * with use_file set, a handle and a time of day other than 1, the values
 * from the file, each the next line not starting with '#', '\n' or '\r'
 * (00429020; at the end the line read last stands): 8 decimals (00431f38)
 * to vert offset near / far, z offset near / far, height near / far,
 * radius near / far; two colours "r g b" (00429098 / 00429050 tokens,
 * atoi, r << 10 | g << 5 | b); ring, ring y offset, ring height; three
 * groups of five (the fifth << 8); the final flag, which sets the ring;
 * then closed. Otherwise closed (the handle as it is) and the stem's row:
 * cl1, Cl1 .. cy3, Cy3 exactly, tr1..tr7 in any case (00488723), each
 * match taking the row (an unknown stem: none, port guard: the values left
 * as they were), the decimals << 16, the colours packed, the rest as
 * stored, without forcing the ring. Then with screen mode 0 the ring's y
 * offset and height x 200 / 480 (32-bit, toward zero); sky_flag = the
 * final flag, 0 when the stem does not start with 't' and the time of day
 * is 1; the tint from the time of day's group; 004fccf0 = 0. */
void track_info_load(const TrackInfoOps *ops, const TrackInfoInputs *in, TrackInfo *info);


/* 00431f38: a decimal as 16.16 ("-45" or "0.25"), parsed in place: the dot
 * becomes a NUL, and past four digits after the leading zeros another. */
int32_t track_info_decimal(char *text);

enum { TRACK_STEM = 0x40 };  /* 004fd0f4..004fd133 (004fd134 is another word) */

/* 0043e9b4 (watcall (name), from 004275f8 with the setup's name 004fb7c6):
 * the name to the track's stem 004fd0f4, NUL and all. Port guard: within
 * TRACK_STEM bytes (the original copies on). */
void track_stem_set(char stem[TRACK_STEM], const char *name);

/* track_info_load on a file's text (high resolution, time of day 0, no
 * tint); 0 for an empty text. */
int track_info_parse(TrackInfo *info, const char *text, size_t size);

#endif
