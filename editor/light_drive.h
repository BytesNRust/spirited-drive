/* The Track light window's test drive (the light tuning design, piece 1): the game run in a window of its own on a CD track at one node and one time of day, with the light
 * editor on (TNFS_LIGHT_EDIT=1: the aim's cross, the node's values, F10 holds the car, F11 copies a brightness line) and the working copy of the light and places files in front
 * of the real ones (a scratch folder's mods/, as the photo does). Every edit rewrites the copy whole and the game, which looks at its light file once a second, makes the
 * light again: no Save to see a change. One game at a time; the scratch folder goes when the run ends. The command is a pure function; the job runs the game in the
 * background (light_photo.h's scratch folder, car_race.h's process runner). POSIX only, no window of Studio's. */
#ifndef TNFS_TRACK_EDITOR_LIGHT_DRIVE_H
#define TNFS_TRACK_EDITOR_LIGHT_DRIVE_H

#include "car_race.h"
#include "proc_run.h"

#include <stddef.h>

enum { LIGHT_DRIVE_WHY = 256, LIGHT_DRIVE_WEATHERS = 4 };

/* the game's weathers, as TNFS_WEATHER says them: 0 clear (the default), 1 fog, 2 rain, 3 storm. NULL out of range. */
const char *light_drive_weather_word(int weather);   /* "clear", "fog", "rain", "storm" */
const char *light_drive_weather_name(int weather);   /* "Clear", "Fog", "Rain", "Storm" */
int light_drive_weather_clamp(int weather);          /* weather when it is in range, else 0 */
/* the person's weather, kept for the session like the photo's view */
void light_drive_weather_pref_set(int weather);
int light_drive_weather_pref(void);

typedef struct {
    const char *game;        /* the game's program */
    const char *cd;          /* the game's files (--data), only read */
    const char *user;        /* the scratch player folder (port.cfg and restomod.cfg are in it) */
    const char *mods;        /* the folder with track_light/ and track/: the working copies, which go first */
    char track[8];           /* a CD track's stem: "CY1" */
    char letter;             /* the time of day: 'A', 'B', 'C' or 'N' */
    unsigned node;           /* the road node the car starts at (TNFS_START_NODE) */
    int weather;             /* 0..LIGHT_DRIVE_WEATHERS-1 */
    const char *masks;       /* NULL for none, else a folder of <TRACK>/<name>.png in front of the game's own track masks (TNFS_TRACK_MASKS) */
    const char *window;      /* NULL or "" for the game's own place, else "X,Y": where its window opens (TNFS_WINDOW_POS), beside Studio */
    int view;                /* the preview's camera (light_photo.h's view 0..LIGHT_VIEWS-1): the game's chase camera the drive starts in (TNFS_VIEW), the person can change it */
} LightDriveSetup;

/* The command line and environment of the drive in `out` (malloc'd words: car_race_command_free). `base` is the environment it starts from (NULL terminated, NULL for none): its
 * TNFS_* and SDL_* are dropped, so a variable of the person's cannot change the drive; there is no SDL_* of ours either: the window is real. 1, or 0 with `why`. */
int light_drive_command(const LightDriveSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap);

typedef enum { LIGHT_DRIVE_IDLE = 0, LIGHT_DRIVE_RUNNING, LIGHT_DRIVE_ENDED, LIGHT_DRIVE_FAILED } LightDriveState;

typedef struct {
    LightDriveState state;   /* ENDED: the game's window was closed (exit 0); FAILED: it ended another way, `why` says how */
    ProcRun run;
    RaceCommand command;
    char temp[EXPORT_PATH];  /* the scratch folder while the run lasts, "" after */
    char mods[EXPORT_PATH + 16];
    char track[8];
    char letter;
    unsigned node;
    int weather;
    char masks[EXPORT_PATH]; /* set before light_drive_start: a folder of this track's mask PNGs (flat, <name>.png) the drive shows instead of the game's own; "" for none */
    char window[24];         /* set before light_drive_start: "X,Y" for the game's window, "" for its own place */
    char why[LIGHT_DRIVE_WHY];
} LightDriveJob;

/* Starts the drive: the scratch folder with `file_text` as mods/track_light/<TRACK>.txt (the track's one light file; `letter` is the time), the player files, and the
 * game. A drive that runs is stopped first. 1, or 0 with `why` (and the state FAILED). */
int light_drive_start(LightDriveJob *job, const char *game, const char *cd, const char *track, char letter, unsigned node, int weather, const char *file_text, size_t file_size,
                      char *why, size_t why_cap);
/* an edit: the working copy rewritten whole (a temporary file, then a rename, so the game never reads half a file); 1, or 0 with
 * `why` (no drive runs, a write failed). */
int light_drive_update(LightDriveJob *job, const char *file_text, size_t file_size, char *why, size_t why_cap);
/* every frame: takes the game's output and sees whether it ended. 1 while it runs; when it ends the state is ENDED or FAILED and the scratch folder is gone. */
int light_drive_poll(LightDriveJob *job);
/* ends the run (SIGTERM to the game's group), removes the scratch folder; the state is FAILED "stopped" */
void light_drive_stop(LightDriveJob *job);
/* stops a run, frees everything */
void light_drive_free(LightDriveJob *job);

#endif
