/* Where Studio looks for the game's program (editor/main.c, app.c): the places in order, kept apart from SDL and the file system so tests can
 * lay out any Mac. Studio is opened from the Finder with / as its working directory and may run from a translocated copy, so it never trusts
 * the working directory first. */
#ifndef TNFS_EDITOR_GAME_FIND_H
#define TNFS_EDITOR_GAME_FIND_H

#include <stddef.h>

enum { GAME_FIND_PATH = 1024, GAME_FIND_TRIED = 2048 };

/* the game's program inside its app, and its bundle identifier (tools/package_macos.sh) */
#define GAME_FIND_APP_EXE "Spirited Drive.app/Contents/MacOS/tnfs"
#define GAME_FIND_BUNDLE_ID "com.bytesnrust.spirited-drive"

typedef struct {
    const char *base;         /* SDL_GetBasePath(): "<Applications>/Spirited Drive Studio.app/Contents/Resources/" in the bundle; NULL when unknown */
    const char *home;         /* the user's home folder ending in '/', NULL when unknown */
    const char *remembered;   /* the program the user pointed at last time (Locate game), NULL for none */
    int (*is_file)(const char *path, void *ctx);                    /* a file at `path` */
    int (*by_bundle_id)(char *app, size_t cap, void *ctx);          /* the game's .app wherever the system knows it, 1 and `app` filled; NULL for none */
    void *ctx;
} GameFinder;

/* The game's program: the remembered one, the app beside this Studio (<base>/../../../Spirited Drive.app, not when this Studio runs from an App
 * Translocation copy, where the neighbour is hidden), /Applications, ~/Applications, the app the system knows by its bundle id, and last ./tnfs as a
 * build tree has it. 1 with the first that is a file in `out`; 0 when none is (`out` is then "./tnfs", the build tree's). `tried`, when not NULL,
 * gets every place looked at, ", "-separated. */
int game_find(const GameFinder *f, char *out, size_t cap, char *tried, size_t tried_cap);

/* 1 when `path` is the program inside a Spirited Drive app: ends in ".app/Contents/MacOS/tnfs" */
int game_find_is_app_program(const char *path);

/* what the user picked in Locate game: an app folder ("…/Spirited Drive.app", with or without a final '/') turns into its program, a path that is
 * already the program stays; 0 for anything else (NULL, too long) */
int game_find_program_of(const char *picked, char *out, size_t cap);

#endif
