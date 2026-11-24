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

/* Takes entry `index` out of the list (the older ones move up); an index that is not in the list changes nothing. */
void recent_remove(Recent *r, int index);

/* Takes out every entry of `kind` (RECENT_TRACK or RECENT_CAR), the others keep their order. */
void recent_clear(Recent *r, int kind);

/* 1 when `dir` is `folder` or lies below it, both read through their real paths when they exist (the list holds real paths, a workspace may be named through a link);
 * an empty `folder` holds everything. A name that only starts the same ("/ws-two" for "/ws") is not inside. */
int recent_inside(const char *dir, const char *folder);

/* Writes the list to `file`, through a temporary file. 1 on success. */
int recent_save(const Recent *r, const char *file);

/* 1 when `dir` still holds a track project (a project.ini in it). */
int recent_is_project(const char *dir);

/* 1 when `dir` is a mod installed in the player's folder (owner 2026-11-24): a folder with a mod.ini directly in a folder called mods or mods_backup, outside `workspace` (a workspace's own folders are never one; "" for none). Studio opens projects, not installed
 * mods; the name of the mod's folder goes to `id`. Nothing is written or changed. */
int installed_mod_of(const char *dir, const char *workspace, char *id, size_t id_cap);
/* the refusal's words for the mod `id` */
#define INSTALLED_MOD_REFUSAL "installed mods are not edited here: %s is installed in the player's mods folder; Studio opens projects, not installed mods (ask its author for the project)"

/* 1 when a recent folder of `kind` is still there: a track's project.ini, a car's kit (a mod.ini and a model/ in it). The cheap question the lists ask every frame. */
int recent_present(const char *dir, int kind);


#endif
