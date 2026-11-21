/* The Track light window's comparison pictures (dep98 step 2, part D): BEFORE, the photo of the files as they are on disk beside the photo of the working copy, and FOUR, the
 * photos of the four times of day at the same node (the working copy for the time being edited, the light folder's files for the others). One game run at a time: the
 * comparison waits for the window's own photo, then takes its pictures one by one, and a picture is made again only when what it shows changed (light_compare_key). No window,
 * POSIX only. */
#ifndef TNFS_EDITOR_LIGHT_COMPARE_H
#define TNFS_EDITOR_LIGHT_COMPARE_H

#include "light_photo.h"
#include "light_session.h"

#include <stdint.h>

enum { LIGHT_COMPARE_OFF = 0, LIGHT_COMPARE_BEFORE, LIGHT_COMPARE_FOUR, LIGHT_COMPARE_MODES, LIGHT_COMPARE_SLOTS = 4 };

typedef struct {
    int mode;                                  /* LIGHT_COMPARE_* */
    LightPhotoJob job[LIGHT_COMPARE_SLOTS];    /* BEFORE: job[0], the saved files at the node; FOUR: A, B, C, N in that order */
    uint64_t shown[LIGHT_COMPARE_SLOTS];       /* the key of the picture each job holds, 0 for none */
    uint64_t started[LIGHT_COMPARE_SLOTS];     /* the key the job in flight was started for */
    int running;                               /* the slot whose game runs, -1 for none */
    int missing[LIGHT_COMPARE_SLOTS];          /* 1: the light folder has no file for that slot's time (a picture cannot be made) */
    char why[LIGHT_PHOTO_WHY];                 /* the last refusal or failure, "" for none */
    char masks[EXPORT_PATH];   /* the track's mask PNGs of the workspace (flat) the pictures show in front of the game's own; "" for none (set by the App) */
} LightCompare;

void light_compare_init(LightCompare *c);
void light_compare_free(LightCompare *c);

/* how many pictures a mode makes (0, 1, 4) and the time letter of a slot in FOUR ('A'..'N') */
int light_compare_count(int mode);
char light_compare_letter(int slot);

/* What a picture shows, as a number that changes when the picture would: the track, time, node, view and the bytes of the light file and the places file it was made with.
 * `file` and `places` may be NULL (none). Never 0. */
uint64_t light_compare_key(const char *track, char letter, unsigned node, int view, int hud, const char *file, size_t file_size, const char *places, size_t places_size);

/* Every frame: takes the running game's output; when none runs and `main_busy` is 0, starts the first picture of the mode whose key is out of date. `node`, `view` and `hud` are the window's
 * photo node, view and HUD. 1 while a game runs or a picture is still due, 0 when every picture is current (or the mode is off). The game and the CD data are the window's. */
int light_compare_poll(LightCompare *c, const LightSession *s, int node, int view, int hud, const char *game, const char *cd, int main_busy);

#endif
