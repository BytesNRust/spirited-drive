/* The Track light window's session (dep98 step 2, part C): one light file of one track and time open for tuning: the working copy (a LightFile, edited through light_file.h),
 * what was on disk when it was opened or last saved, and the rules of Save. Save writes only <light folder>/track_light/<TRACK><letter>.txt, and only into a light folder
 * that has a track_light/ and is not under an `original/` folder (the CD's data is only read); there is no other write. Pure C and POSIX files, no window. */
#ifndef TNFS_EDITOR_LIGHT_SESSION_H
#define TNFS_EDITOR_LIGHT_SESSION_H

#include "light_file.h"

#include <stddef.h>

enum { LIGHT_SESSION_PATH = 4096 };

typedef struct {
    char dir[LIGHT_SESSION_PATH];   /* the light folder: the one whose track_light/ is tuned */
    char track[8];
    char letter;                    /* 'A', 'B', 'C' or 'N' */
    int has_file;                   /* 0 when the folder has no file for this track and time (TR5's light is made in code): nothing to edit or save */
    LightFile file;                 /* the working copy; edit it with light_file_set_*; valid only with has_file */
    char *loaded;                   /* the file's bytes as read or last saved */
    size_t loaded_size;
} LightSession;

/* Opens <dir>/track_light/<TRACK><letter>.txt. 1 when the folder is a light folder; with no file there, has_file is 0 and `why` says so. 0 with `why` when the folder has no
 * track_light/ or is under an `original/` folder, the track is not a stem (two capitals and a digit), the letter is not A, B, C or N, or the file is not a light file of that track and time. */
int light_session_open(LightSession *s, const char *dir, const char *track, char letter, char *why, size_t cap);
void light_session_close(LightSession *s);

/* The file's path in the light folder, "" without a file. */
void light_session_path(const LightSession *s, char *out, size_t cap);
/* The working copy's bytes (malloc'd, NUL-terminated; NULL without a file or out of memory), as the photo job takes them. */
char *light_session_text(const LightSession *s, size_t *size);
/* 1 when the working copy is not what is on disk (the bytes differ), 0 otherwise. */
int light_session_dirty(const LightSession *s);
/* The working copy goes back to what was read or last saved. */
void light_session_revert(LightSession *s);
/* A node of the photo kept inside the road: 0 .. nodes-1 (0 without a file). */
unsigned light_session_clamp_node(const LightSession *s, long node);
/* Writes the working copy as the file (a temporary file next to it, then a rename) and makes it the new baseline. 1, or 0 with `why` (no file, a folder that is no longer a light
 * folder, a write that failed); the file on disk is then as it was. */
int light_session_save(LightSession *s, char *why, size_t cap);

#endif
