/* port (track light, one file a track): data/track_light/<TRACK>.txt holds the facts of a track (where its tunnels and halls are, whether it snows) and, in one block for each time of day, how that
 * time looks: the sun, the tunnels' look, the lamps, the retouches, and `rows` for the nodes whose rows are written by hand (the other rows are made when the track loads). The format is in
 * notes/track-light-format-2026-11-22.md; a line is one of
 *
 *   track CY1 | tunnel A B [lit | gradient N] | snow 0|1 | snow_map FROM TO                          the facts, before the first `time`
 *     gradient N: the nodes inside each mouth of the tunnel over which its light falls from the open road's to the tunnel's, 0 a straight cut, 1..8, 3 when the line has none
 *   time A|B|C|N                                                                       a time's block runs to the next `time`
 *   sun AZ EL SHADOW                      degrees, degrees, percent of light left in full shadow; a part may be `-` (the generator's)
 *   tunnel_look [A B] [LIGHT] [COLOUR] [lamps S [COLOUR]] [reach R]
 *   lamp NODE PERCENT [COLOUR[:S]] [RADIUS]     (STRIP: the last word, where across the road it stands)
 *   brightness A B PERCENT [STRIP] | tone A B [STRIP] RRGGBB:PERCENT | rows A B p0..p10
 *
 * STRIP: `pA pB` (the points A..B of the row, inclusive), `pA`, `p1,p3,p7` (a list: any points), or `left` (p6..p10), `right` (p1..p5), `road` (p0 p1 p6), `all` (p0..p10). */
#ifndef TNFS_TRACK_LIGHT_TRACK_H
#define TNFS_TRACK_LIGHT_TRACK_H

#include "game/race/track_light_file.h"

/* The file in `text` for `track` as the time `letter` (A, B, C or N) sees it, every line of every block checked: 1 with `out` taking the facts (light.tunnel, light.snow, light.snow_map, from_places 1), the
 * time's tweaks, its `rows` overrides (light.data and row_set, both nodes long) and its sun; 0 when the file is for another track (*bad_line 0) or a line is bad (its number in *bad_line). `out` is untouched
 * unless 1. */
int track_light_track_parse(const char *text, size_t size, const char *track, char letter, uint32_t nodes, TrackLightFile *out, int *bad_line);

/* where the track file `<TRACK>.txt` is (a mod's first, then the data folders), 1 when there is one; and its parse for a time, as track_light_load is for the old files */
int track_light_track_find(const char *track, char *out, size_t out_size);
int track_light_track_load(const char *track, char letter, uint32_t nodes, TrackLightFile *out, char *path, size_t path_size);

/* the track's facts (its tunnels, snow and snow_map: the same at every time of day) of its file into `out`, which has no tunnel array: 1 when there is a file and it is well-formed */
int track_light_track_places(const char *track, uint32_t nodes, TrackNodeLight *out);

#endif
