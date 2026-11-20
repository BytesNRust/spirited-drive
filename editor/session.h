/* What the editor remembers between projects and between runs: the recently opened project folders (a file in the editor's own folder) and a
 * fingerprint of a trace, so the editor knows whether there is anything unsaved. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_SESSION_H
#define TNFS_TRACK_EDITOR_SESSION_H

#include "project.h"

#include <stdint.h>

enum {
    RECENT_MAX = 8,
    SESSION_PATH = 4096
};

/* what a recent folder holds: tracks and cars are listed together */
enum { RECENT_TRACK = 0, RECENT_CAR = 1 };

typedef struct {
    char dir[RECENT_MAX][SESSION_PATH];   /* the newest first */
    int kind[RECENT_MAX];                 /* RECENT_TRACK or RECENT_CAR */
    int count;
} Recent;

/* Reads the list from `file` (one folder a line, with "car" and a tab before a car's; a line with no mark is a track's, as the first Studio wrote them);
 * a missing or unreadable file is an empty list. */
void recent_load(Recent *r, const char *file);

/* Puts `dir` first, of `kind`: it leaves the place it had (whatever kind it was), and the oldest falls off past RECENT_MAX. An empty `dir` is ignored. */
void recent_push(Recent *r, const char *dir, int kind);

/* Writes the list to `file`, through a temporary file. 1 on success. */
int recent_save(const Recent *r, const char *file);

/* 1 when `dir` still holds a track project (a project.ini in it). */
int recent_is_project(const char *dir);

/* 1 when a recent folder of `kind` is still there: a track's project.ini, a car's mod.ini (in it or in its coche/). The cheap question the lists ask every frame. */
int recent_present(const char *dir, int kind);

/* A fingerprint of the trace (its points, its width and its road keys) for the unsaved-changes check only: it differs when any changed. Not a piece's print
 * (records and replays use track_piece_fingerprint, over the built files). */
uint64_t session_fingerprint(const Trace *trace);

#endif
