/* The workspace: one folder with cars/ and tracks/ in it (<workspace>/cars/<kit>, <workspace>/tracks/<id>), chosen once in the File menu and kept in a small file
 * beside the recent projects. With one set, Open and New start in it and the start screen lists what it holds. Optional: with none, Studio asks the system
 * where to start, as before. Pure C: the "is this folder a project" question is passed in, so the tests answer it themselves. */
#ifndef TNFS_EDITOR_WORKSPACE_H
#define TNFS_EDITOR_WORKSPACE_H

#include <stddef.h>

enum { WORKSPACE_KIND_CAR = 0, WORKSPACE_KIND_TRACK = 1 };
enum { WORKSPACE_LIST = 96, WORKSPACE_NAME = 96 };

/* what the workspace holds: the names of the folders in cars/ and tracks/ that are projects, sorted without regard to case; a folder past WORKSPACE_LIST is left out */
typedef struct {
    char car[WORKSPACE_LIST][WORKSPACE_NAME];
    int cars;
    char track[WORKSPACE_LIST][WORKSPACE_NAME];
    int tracks;
} WorkspaceList;

/* the question "is `dir` a project of this kind" (WORKSPACE_KIND_*) */
typedef int (*WorkspacePresent)(const char *dir, int kind);

/* The folder `dir` made ready: it must be a folder; cars/ and tracks/ are made in it when missing. 1 when it is; 0 with `why` set. */
int workspace_prepare(const char *dir, char *why, size_t why_cap);

/* The projects in cars/ and tracks/ of `dir`. */
void workspace_scan(const char *dir, WorkspacePresent present, WorkspaceList *out);

/* <dir>/cars/<name> or <dir>/tracks/<name> into `out`; 0 when it does not fit */
int workspace_path(const char *dir, int kind, const char *name, char *out, size_t cap);

/* Where a folder dialog of `kind` starts: the workspace's cars/ or tracks/ when there is a workspace, else the folder that holds the project that is open (`open_dir`'s
 * parent), else "" (the system's own place). */
void workspace_start_dir(const char *dir, int kind, const char *open_dir, char *out, size_t cap);

/* The kept choice: the workspace folder ("" for none) and whether the offer to choose one was put aside. A missing file is no workspace and not put aside. */
void workspace_load(const char *file, char *dir, size_t cap, int *put_aside);
int workspace_save(const char *file, const char *dir, int put_aside);

#endif
