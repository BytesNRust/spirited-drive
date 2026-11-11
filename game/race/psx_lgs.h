/* port: the PS1 look's light script. Z<TRACK><A|B|C>.LGS is text: C comments, then the depth cue's
 * four points (X,Y percent), the default ambient colour at the start and at the end (128 is neutral, per channel) and the
 * light contexts. Only the ambient colour is read here; the contexts (the cars' light by segment range) are not. Numbers are
 * separated by anything that is not a digit, so commas, spaces and CR / LF all do, and the file's trailing binary after the
 * last line ("CRCF" and padding) ends the read. */
#ifndef TNFS_PSX_LGS_H
#define TNFS_PSX_LGS_H

#include <stddef.h>
#include <stdint.h>

enum { PSX_LGS_CONTEXTS = 32, PSX_LGS_FIXED = 64, PSX_LGS_RAMP = 96, PSX_LGS_BLEND = 2 };

typedef struct {
    int seg_ini, seg_fin;   /* the segments the context covers, both ends included */
    int mode;               /* PSX_LGS_FIXED: c1 throughout; PSX_LGS_RAMP: c1 at seg_ini to c2 at seg_fin */
    int c1[3], c2[3];
} PsxLgsContext;

typedef struct {
    int depth[8];       /* X0,Y0 .. X3,Y3 */
    int start[3];       /* the ambient colour at the track's start: r, g, b */
    int end[3];         /* and at its end */
    int context_count;  /* the contexts read (those after PSX_LGS_CONTEXTS are dropped; none when the file stops early) */
    PsxLgsContext contexts[PSX_LGS_CONTEXTS];
} PsxLgs;

/* 1 and `out` set when the text has the depth cue and both ambient colours (each channel 0..255); 0 otherwise. */
int psx_lgs_parse(const uint8_t *file, size_t size, PsxLgs *out);

/* The ambient colour the cars and the dash have at `segment` of a track of `segment_count`, as a multiplier of each channel (1.0
 * for 128): inside a context its colour (fixed, or ramped across its range), else the start colour ramped to the end across the
 * track; PSX_LGS_BLEND segments outside a context its colour fades in and out into the default one. */
void psx_lgs_ambient(const PsxLgs *lgs, int segment, int segment_count, float out[3]);

/* 1 when `segment` is inside a PSX_LGS_RAMP context (kUseAC + kSwitchAC, 96): the tunnels, where the ceiling lamps switch the cars' light;
 * the edges are exact, no blend. 0 for NULL. */
int psx_lgs_switched(const PsxLgs *lgs, int segment);

#endif
