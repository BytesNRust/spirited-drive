/* The textures of an original track (dep103): the road texture slots and the scenery boards a track's reflective masks are painted over, read from the game's own files
 * (<cd>/SIMDATA/ETRACKFM/<TRACK>_001.FAM, the midday file) with the game's loader (game/race/track_art.h). Named as the masks are: "slot_07" (slot = id * 3 + letter, variant 0)
 * and "scenery_02". As RGBA through the track's own palette, colour 0xFF transparent. Pure C, no window; the CD's files are only read. */
#ifndef TNFS_EDITOR_TRACK_TEX_H
#define TNFS_EDITOR_TRACK_TEX_H

#include <stddef.h>
#include <stdint.h>

enum { TRACK_TEX_NAME = 16, TRACK_TEX_MAX = 256 };

typedef struct {
    char name[TRACK_TEX_NAME];   /* "slot_07", "scenery_02" */
    int width, height;
} TrackTexInfo;

typedef struct TrackTexSet TrackTexSet;

/* Opens the track's art: NULL with `why` when the file is not there or does not read. `track` is a stem ("CL1"), `cd` the game's files. */
TrackTexSet *track_tex_open(const char *cd, const char *track, char *why, size_t why_cap);
void track_tex_close(TrackTexSet *set);

/* the textures in order: the slots by number, then the boards by number */
int track_tex_count(const TrackTexSet *set);
const TrackTexInfo *track_tex_info(const TrackTexSet *set, int index);

/* One texture as RGBA (width x height x 4, malloc'd: free() it); 1, or 0 when the set has none of that name. */
int track_tex_rgba(const TrackTexSet *set, const char *name, int *width, int *height, uint8_t **rgba);

#endif
