/* Test in race and Race photo (dep59 car editor E2): the game (the one Studio found) run on a scratch player folder with the project's masks in front of its own
 * (TNFS_CAR_MASKS, TNFS_DASH_MASKS), never on the player's own folder and never on the installed game. Kinds:
 *  - a drivable original car: the player drives it (--car N, alone on the track);
 *  - a traffic car or the police car: a race with traffic (--type 1, on a stage track: the circuits have none) and the camera following the slot the car has in it.
 *    The traffic's models are drawn by the race's seed, so Studio tries seeds (TNFS_DRIVE_SEED) until the car is in the draw; the game's own log says which car is in
 *    which slot ("race car: slot N is STEM").
 * Day or Night (Night is light_time=night and light_effects=1). The command line and the environment are built by pure functions; the job runs them in the
 * background and keeps the log and the photo. POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_CAR_RACE_H
#define TNFS_TRACK_EDITOR_CAR_RACE_H

#include "car_export.h"
#include "car_project.h"
#include "proc_run.h"

#include <stddef.h>
#include <stdint.h>

enum {
    RACE_ARGS = 24, RACE_ENVS = 1024,   /* words of a command line, and of an environment (a shell's has a hundred or so) */
    RACE_WORD_MAX = 8192,
    RACE_TICKS_DRIVEN = 200,  /* the photo is the frame after this many ticks: a car you drive (the car card's node) */
    RACE_TICKS_TRAFFIC = 800, /* a traffic car, after the start has put it on the road (found by looking at frames of the follow camera on slots 3 and 4: not on screen before 650) */
    RACE_TICKS_POLICE = 250,  /* the police car, put behind the player chasing (RACE_POLICE_NEAR nodes), so the camera on it has it at any tick */
    RACE_POLICE_NEAR = 15,
    RACE_SEEDS = 24,          /* the seeds tried to find a traffic car in the draw */
    RACE_SEED_FIRST = 0x2b68, /* the game's own fixed seed is 0x2b67; the seeds above it draw other models, the small ones draw one model for every slot */
    RACE_SLOTS = 9,
    RACE_LOG_MAX = 131072,
    RACE_PHOTO_W = 640, RACE_PHOTO_H = 480
};

typedef struct { const char *stem, *label; int stage; } RaceTrack;
/* the tracks of the picker: the stages (City, Coastal, Alpine: traffic runs there) first, then the circuits */
int car_race_track_count(void);
const RaceTrack *car_race_track(int index);
/* the index of the track with this stem (any case), -1 for none */
int car_race_track_find(const char *stem);
/* the ticks of a car's photo */
int car_race_photo_ticks(const ExportCar *car);

/* the track a car starts on: Rusty Springs for a drivable car (the night reference of the design), Coastal 1 for traffic (circuits have none) */
int car_race_default_track(const ExportCar *car);

typedef struct {
    const char *game;        /* the game's program */
    const char *cd;          /* the game's files (--data) */
    const char *project;     /* the project's folder: car_masks/ (and dash_masks/) are in it */
    const char *user;        /* the scratch player folder */
    const ExportCar *car;
    const char *track;       /* a stem of the table above */
    int night;
    int slot;                /* the slot the camera follows (TNFS_FOLLOW_CAR); 0 for the player's */
    uint32_t seed;           /* TNFS_DRIVE_SEED; 0 for the game's own */
    int photo_ticks;         /* > 0: headless, the frame after that many ticks in `shot`; 0: a race in a window */
    const char *shot;
} RaceSetup;

typedef struct {
    char *argv[RACE_ARGS + 1];   /* malloc'd words, NULL terminated */
    char *envp[RACE_ENVS + 1];
    int argc, envc;
} RaceCommand;

/* The command line and environment of a run in `out` (its words are malloc'd: car_race_command_free; a struct that is built again is freed first). `base` is the environment it starts from (NULL terminated;
 * NULL for none): its TNFS_* are dropped (so a variable of the person's cannot change the test) and, for a photo, its SDL_* too. 1, or 0 with `why`: the car is a
 * traffic or police car and the track has no traffic, a path is missing. */
int car_race_command(const RaceSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap);
void car_race_command_free(RaceCommand *c);

/* the scratch player folder's files: port.cfg (Restomod) and restomod.cfg (no extra liveries, render scale 1, the time, and for the night light_effects=1), into the
 * existing folder `user` */
int car_race_user_files(const char *user, int night, char *why, size_t why_cap);

/* a scratch folder "<TMPDIR>/studio-race-XXXXXX" with a user/ folder in it: `temp` and `user` filled; 0 with `why` */
int car_race_temp(char *temp, size_t temp_cap, char *user, size_t user_cap, char *why, size_t why_cap);
/* removes a scratch folder car_race_temp made (only a folder whose last part starts "studio-race-"; links are not followed); 1 when it is gone */
int car_race_temp_remove(const char *temp);

/* the slot a car has in the game's log ("race car: slot N is STEM", the stem in any case): N, or -1 when the log does not have it */
int car_race_find_slot(const char *log, const char *stem);

/* how a line of the game's output should look */
typedef enum { RACE_LINE_PLAIN = 0, RACE_LINE_ERROR, RACE_LINE_MASK, RACE_LINE_NOTE } RaceLineKind;
/* ERROR: a line that says error, fatal, cannot, failed, no such, not found, bad or a crash (any case); MASK: "car mask:" (which file a texture took); NOTE: the game's
 * "cannot read .../<tex>_mask.png" for a car the project has no masks for (it reads the override first for every car, then its own data): expected, shown dim; else PLAIN */
RaceLineKind car_race_line_kind(const char *line, size_t len);

/* how many lines of `text` are RACE_LINE_NOTE: the panel shows them as one summary line */
int car_race_count_notes(const char *text, size_t len);

/* a binary PPM (P6, 640 x 480, 255) as RGBA: `rgba` malloc'd; 0 with `why` */
int car_race_read_ppm(const char *path, uint8_t **rgba, int *w, int *h, char *why, size_t why_cap);

typedef enum { RACE_JOB_IDLE = 0, RACE_JOB_PROBE, RACE_JOB_SHOOT, RACE_JOB_RACE } RaceJobState;

typedef struct {
    RaceJobState state;
    ProcRun run;
    RaceCommand command;
    RaceSetup setup;                 /* its strings are in `own` */
    char own[5][EXPORT_PATH];        /* game, cd, project, user, shot */
    char track[8];
    char temp[EXPORT_PATH];
    char *log;                       /* what the finished runs wrote (and the job's own lines), RACE_LOG_MAX at most; the run's own is run.log */
    size_t log_len;
    int probes;                      /* seeds tried */
    int finished;                    /* the job ended (ok or not): `summary` says how */
    int ok;
    char summary[400];
    uint8_t *photo;                  /* RGBA, RACE_PHOTO_W x RACE_PHOTO_H; NULL for none */
    int photo_serial;                /* changes when `photo` does */
    int photo_slot, photo_night;     /* what the photo shows */
    uint32_t photo_seed;
    char photo_track[8];
} RaceJob;

/* Starts Test in race (a race in the game's window) or Race photo (headless, then the picture in job->photo) of `car` from `project`. `game` and `cd` are the game's
 * program and files. 1 when the job started, 0 with `why` (the car cannot race there, no game, a scratch folder that cannot be made). Refused while a job runs. */
int car_race_start_race(RaceJob *job, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, int night, char *why, size_t why_cap);
int car_race_start_photo(RaceJob *job, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, int night, char *why, size_t why_cap);
/* every frame: takes the output, moves a photo job from the probes to the shot, ends the job; 1 while it runs */
int car_race_poll(RaceJob *job);
void car_race_stop(RaceJob *job);
int car_race_busy(const RaceJob *job);
void car_race_free(RaceJob *job);

/* the ExportCar a project is of (its stem for a car project, its masks folder for a masks project); NULL for a car that is not one of the game's */
const ExportCar *car_race_original_of(const CarProject *c);

#endif
