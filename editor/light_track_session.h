/* The Track light window's session on the new light file (light_track.h): one file of one track, data/track_light/<TRACK>.txt, open for tuning: the working copy, what was on
 * disk when it was opened or last saved, and the rules of Save. Save writes only <light folder>/track_light/<TRACK>.txt, and only into a light folder that has a track_light/ and
 * is not under an `original/` folder (the CD's data is only read); there is no other write. The file does not say how many nodes the track has: the caller gives the TRI's
 * count (original_track_nodes). Pure C and POSIX files, no window. */
#ifndef TNFS_EDITOR_LIGHT_TRACK_SESSION_H
#define TNFS_EDITOR_LIGHT_TRACK_SESSION_H

#include "light_track.h"

#include <stddef.h>

enum { LIGHT_TRACK_SESSION_PATH = 4096 };

typedef struct {
    char dir[LIGHT_TRACK_SESSION_PATH];   /* the light folder: the one whose track_light/ is tuned */
    char track[8];
    unsigned nodes;
    int has_file;                         /* 0 when the folder has no file for this track (TR5's light is made in code): nothing to edit or save */
    LightTrack file;                      /* the working copy; edit it with light_track_set_*; valid only with has_file */
    char *loaded;                         /* the file's bytes as read or last saved */
    size_t loaded_size;
} LightTrackSession;

/* Opens <dir>/track_light/<TRACK>.txt for a track of `nodes` nodes. 1 when the folder is a light folder; with no file there, has_file is 0 and `why` says so. 0 with `why` when the
 * folder has no track_light/ or is under an `original/` folder, the track is not a stem (two capitals and a digit), there are no nodes, or the file is not the light file of that track. */
int light_track_session_open(LightTrackSession *s, const char *dir, const char *track, unsigned nodes, char *why, size_t cap);
void light_track_session_close(LightTrackSession *s);

/* The file's path in the light folder, "" without a file. */
void light_track_session_path(const LightTrackSession *s, char *out, size_t cap);
/* The working copy's bytes (malloc'd, NUL-terminated; NULL without a file or out of memory), as the jobs take them. */
char *light_track_session_text(const LightTrackSession *s, size_t *size);
/* 1 when the working copy is not what is on disk (the bytes differ). */
int light_track_session_dirty(const LightTrackSession *s);
/* The working copy goes back to what was read or last saved. */
void light_track_session_revert(LightTrackSession *s);
/* A node of the photo kept inside the road: 0 .. nodes-1 (0 without a file). */
unsigned light_track_session_clamp_node(const LightTrackSession *s, long node);
/* Writes the working copy as the file (a temporary file next to it, then a rename) and makes it the new baseline. 1, or 0 with `why`; the file on disk is then as it was. */
int light_track_session_save(LightTrackSession *s, char *why, size_t cap);

#endif
