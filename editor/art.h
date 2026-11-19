/* The generic art Studio carries (editor/art/GENERIC.FAM: CC0 textures and our own road lines and sky, no byte of the CD): where the running
 * editor finds it. A new project gets its own copy, so a project folder works on any Mac with nothing of the source tree beside it. */
#ifndef TNFS_TRACK_EDITOR_ART_H
#define TNFS_TRACK_EDITOR_ART_H

#include <stddef.h>

/* The path of GENERIC.FAM beside the running program: in the app bundle (Contents/Resources, whether the program asks for it from there or from
 * Contents/MacOS), or in the program's own folder (a build tree). 1 with `out` filled, 0 when there is none. */
int art_generic_find(char *out, size_t cap);

#endif
