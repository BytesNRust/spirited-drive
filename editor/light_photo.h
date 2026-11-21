/* The Track light window's photo (dep98 step 2, part B): the game run headless on a CD track at one node and one time of day, with the working copy of a light file
 * in front of the real one (a mod's track_light/ and track/ go first), and its frame read back as a picture. The command and its environment are built by a pure
 * function; the job runs the game in the background on a scratch folder (as car_race.h's Race photo does: the same scratch folder, picture reader and process runner),
 * and the scratch folder, with the working copy in it, is removed when the run ends. The CD's own files are only read (the game's --data). POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_LIGHT_PHOTO_H
#define TNFS_TRACK_EDITOR_LIGHT_PHOTO_H

#include "car_race.h"
#include "proc_run.h"

#include <stddef.h>
#include <stdint.h>

enum {
    LIGHT_PHOTO_TICKS_DEFAULT = 4,   /* the car is put at the node, then drives this many ticks before the frame */
    LIGHT_PHOTO_TICKS_MAX = 1000,
    LIGHT_PHOTO_WHY = 256
};

/* The camera of the photo, a kind of the game's chase camera (TNFS_VIEW, game/race/chase_struct.h): the bumper has no car in the picture (the clean track light), the dash is the
 * cockpit, and the chase cameras show the car from 4 m to 16 m behind. A chase camera glides to its place, so the photo of one runs more ticks than the others. */
enum { LIGHT_VIEWS = 6, LIGHT_VIEW_DEFAULT = 0, LIGHT_PHOTO_TICKS_CHASE = 200 };
const char *light_photo_view_name(int view);     /* "Bumper", "Dash", "Chase near", "Chase mid", "Chase far", "High"; NULL out of range */
int light_photo_view_kind(int view);             /* the game's chase kind of view 0..LIGHT_VIEWS-1: 1, 0, 2, 3, 4, 12; -1 out of range */
int light_photo_view_ticks(int view);            /* the ticks the photo of the view runs: LIGHT_PHOTO_TICKS_DEFAULT, or LIGHT_PHOTO_TICKS_CHASE for the chase cameras */
int light_photo_view_clamp(int view);
/* the person's view, kept for the session and in the ini (the window, the photo and the comparison all use it): clamped on the way in */
void light_photo_view_pref_set(int view);
int light_photo_view_pref(void);            /* view when it is in range, else LIGHT_VIEW_DEFAULT */

/* the game's name of a time of day for a light file's letter: 'A' "dawn", 'B' "day", 'C' "dusk", 'N' "night"; NULL for any other */
const char *light_photo_time_word(char letter);

typedef struct {
    const char *game;        /* the game's program */
    const char *cd;          /* the game's files (--data), only read */
    const char *user;        /* the scratch player folder (port.cfg and restomod.cfg are in it) */
    const char *mods;        /* the folder with track_light/ and track/: the working copies, which go first */
    char track[8];           /* a CD track's stem: "CY1" */
    char letter;             /* the time of day: 'A', 'B', 'C' or 'N' */
    unsigned node;           /* the road node the car starts at (TNFS_START_NODE) */
    int view;                /* 0..LIGHT_VIEWS-1: the camera */
    int ticks;               /* 1..LIGHT_PHOTO_TICKS_MAX */
    const char *shot;        /* the PPM the game writes */
} LightPhotoSetup;

/* The command line and environment of one photo in `out` (malloc'd words: car_race_command_free). `base` is the environment it starts from (NULL terminated, NULL for
 * none): its TNFS_* and SDL_* are dropped, so a variable of the person's cannot change the photo. 1, or 0 with `why`: a path or the track is missing or not a stem, the
 * letter is not a time, the ticks are out of range. */
int light_photo_command(const LightPhotoSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap);

typedef enum { LIGHT_PHOTO_IDLE = 0, LIGHT_PHOTO_RUNNING, LIGHT_PHOTO_DONE, LIGHT_PHOTO_FAILED } LightPhotoState;

typedef struct {
    LightPhotoState state;
    ProcRun run;
    RaceCommand command;
    char temp[EXPORT_PATH];      /* the scratch folder while the run lasts, "" after */
    char shot[EXPORT_PATH];
    char track[8];
    char letter;
    unsigned node;
    int view;
    uint8_t *rgba;               /* DONE: the picture, w x h RGBA (malloc'd; the last good one stays through a later failure) */
    int w, h;
    int serial;                  /* changes when `rgba` does */
    char why[LIGHT_PHOTO_WHY];   /* FAILED: the reason, with the game's last line when it said one */
} LightPhotoJob;

/* Starts a photo: the scratch folder with `file_text` as mods/track_light/<TRACK><letter>.txt (and `places_text`, when given, as mods/track/<TRACK>.txt), the player
 * files, and the game. A job that runs is stopped first. 1, or 0 with `why` (and the state FAILED). */
int light_photo_start(LightPhotoJob *job, const char *game, const char *cd, const char *track, char letter, unsigned node, int view, int ticks,
                      const char *file_text, size_t file_size, const char *places_text, size_t places_size, char *why, size_t why_cap);
/* every frame: takes the game's output and sees whether it ended. 1 while it runs; when it ends the state is DONE (the picture in `rgba`) or FAILED (`why`), and the
 * scratch folder is gone. */
int light_photo_poll(LightPhotoJob *job);
/* ends the run (SIGTERM to the game's group), removes the scratch folder; the state is FAILED "stopped" */
void light_photo_stop(LightPhotoJob *job);
/* stops a run, frees everything */
void light_photo_free(LightPhotoJob *job);

#endif
