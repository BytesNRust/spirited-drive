/* A car project: a kit folder such as <workspace>/cars/<kit>/ in the current layout (core/formats/car_kit_paths.h, dep85): mod.ini at its top, model/,
 * car_masks/ (flat: <tex>_mask.png), build/. What makes it a car: a mod.ini with type=car and a stem=, and a model/ folder. The car's file is the build's
 * (build/piece/SIMDATA/CARFAMS/<STEM>.CFM): a kit not built yet is a kit, `built` says whether the file is there. Two shapes more open as they always did:
 * a piece folder with no model/ (what Export original car makes of a car you drive: mod.ini, SIMDATA/CARFAMS/<STEM>.CFM, car_masks/<folder>/), and a masks
 * project (a traffic or police car: project.ini with kind=masks). The old coche/ + modelo/ kit is refused. Read only. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_CAR_PROJECT_H
#define TNFS_TRACK_EDITOR_CAR_PROJECT_H

#include "formats/mod_ini.h"

#include <stddef.h>

enum {
    CAR_PROJECT_PATH = 4096,
    CAR_PROJECT_LIST = 24,    /* names kept of each list; the count says how many there are */
    CAR_PROJECT_NAME = 48
};

typedef struct {
    char root[CAR_PROJECT_PATH];     /* the kit's folder */
    char piece[CAR_PROJECT_PATH];    /* the piece the game installs: root/build/piece for a kit (a masks project: its own folder) */
    char kit[CAR_PROJECT_NAME];      /* the kit folder's name */
    ModInfo info;                    /* mod.ini, as the game reads it (a masks project: only id and name, from its project.ini) */
    int legacy;                      /* a piece folder (mod.ini and SIMDATA/ at its top, masks under car_masks/<folder>/): what Export original writes; not a kit, no model/, nothing is built */
    int masks_only;                  /* a masks project (Export original car of a traffic or police car): project.ini with kind=masks, no card, no PBS: the car's masks only */
    char folder[CAR_PROJECT_NAME];   /* the masks' folder name in the game's data/car_masks/<folder>/: the stem for a kit; a masks project says it */
    char cfm[CAR_PROJECT_PATH];      /* the car file, piece/SIMDATA/CARFAMS/<STEM>.CFM (a masks project: original/<X>.CFM, kept to look at) */
    int built;                       /* the car file is there */
    int has_masks_dir;               /* the masks folder (car_project_masks_dir) is there */
    int mask_count;                  /* the masks in it (PNG files), `mask` the first CAR_PROJECT_LIST by name */
    char mask[CAR_PROJECT_LIST][CAR_PROJECT_NAME];
    int has_model;                   /* root/model/ is there */
    int texture_count;               /* the PNG files of root/model/near/bank: the textures seen close up */
    char texture[CAR_PROJECT_LIST][CAR_PROJECT_NAME];
    int far_count;                   /* the PNG files of root/model/far/bank: the far ones */
} CarProject;

/* Reads the car project at `dir` (a kit's folder). 1 with `out` filled; 0 with `why` (English, for the person: what was looked at and what
 * is missing). */
int car_project_detect(const char *dir, CarProject *out, char *why, size_t why_cap);

/* 1 when `dir` holds a masks project: a project.ini that says kind=masks (editor/car_export.h makes them). It is not a track project, whatever the file is called. */
int car_project_is_masks(const char *dir);

/* The folder of the project's masks: root/car_masks for a kit (flat), piece/car_masks/<folder> for a masks project. */
void car_project_masks_dir(const CarProject *c, char *out, size_t cap);
/* the same relative to the project's folder, for the words on screen: "car_masks" for a kit, "car_masks/<folder>" for the other shapes */
void car_project_masks_rel(const CarProject *c, char *out, size_t cap);

/* 1 when `dir` looks like a car project (a mod.ini in it, or an old coche/mod.ini that detect will refuse in words, or a masks project): the cheap question for a recent list, which asks every frame. car_project_detect judges. */
int car_project_present(const char *dir);

#endif
