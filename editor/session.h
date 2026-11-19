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

typedef struct {
    char dir[RECENT_MAX][SESSION_PATH];   /* the newest first */
    int count;
} Recent;

/* Reads the list from `file` (one folder a line); a missing or unreadable file is an empty list. */
void recent_load(Recent *r, const char *file);

/* Puts `dir` first: it leaves the place it had, and the oldest falls off past RECENT_MAX. An empty `dir` is ignored. */
void recent_push(Recent *r, const char *dir);

/* Writes the list to `file`, through a temporary file. 1 on success. */
int recent_save(const Recent *r, const char *file);

/* 1 when `dir` still holds a project (a project.ini in it). */
int recent_is_project(const char *dir);

/* A fingerprint of the trace (its points and its width): it differs when either changed. */
uint64_t session_fingerprint(const Trace *trace);

#endif
