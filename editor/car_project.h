/* A car project: a kit folder such as art/tnfs-cars/<kit>/ (dep59 car editor 1d). The editor takes the kit's root (it holds coche/, modelo/ and
 * mascaras/) or its coche/ folder (which holds mod.ini): when it is given coche/ and the folder above holds modelo/, the kit is the one above. What makes it a
 * car: a mod.ini with type=car and a stem=, and the car's file SIMDATA/CARFAMS/<STEM>.CFM beside it. car_masks/<STEM>/ (the masks the game reads) may
 * be missing: it is listed as none. Read only: the editor writes nothing here yet. Pure C, no window. */
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
    char root[CAR_PROJECT_PATH];     /* the kit: where modelo/ and mascaras/ are (the piece's own folder when it has no kit round it) */
    char piece[CAR_PROJECT_PATH];    /* the folder that holds mod.ini */
    char kit[CAR_PROJECT_NAME];      /* the kit folder's name */
    ModInfo info;                    /* mod.ini, as the game reads it (a masks project: only id and name, from its project.ini) */
    int masks_only;                  /* a masks project (Export original car of a traffic or police car): project.ini with kind=masks, no card, no PBS: the car's masks only */
    char folder[CAR_PROJECT_NAME];   /* the folder of the masks below car_masks/, as the project spells it (the game's data/car_masks/<folder>/) */
    char cfm[CAR_PROJECT_PATH];      /* the car file, piece/SIMDATA/CARFAMS/<STEM>.CFM (a masks project: original/<X>.CFM, kept to look at) */
    int has_masks_dir;               /* piece/car_masks/<STEM>/ is there */
    int mask_count;                  /* the masks in it (PNG files), `mask` the first CAR_PROJECT_LIST by name */
    char mask[CAR_PROJECT_LIST][CAR_PROJECT_NAME];
    int has_modelo;                  /* root/modelo/ is there */
    int texture_count;               /* the PNG files of root/modelo/c1: the textures seen close up */
    char texture[CAR_PROJECT_LIST][CAR_PROJECT_NAME];
    int far_count;                   /* the PNG files of root/modelo/c3: the far ones */
    int kit_mask_count;              /* the PNG files of root/mascaras: the author's colour masks */
    char kit_mask[CAR_PROJECT_LIST][CAR_PROJECT_NAME];
} CarProject;

/* Reads the car project at `dir` (a kit's root or its coche/). 1 with `out` filled; 0 with `why` (English, for the person: what was looked at and what
 * is missing). */
int car_project_detect(const char *dir, CarProject *out, char *why, size_t why_cap);

/* 1 when `dir` holds a masks project: a project.ini that says kind=masks (editor/car_export.h makes them). It is not a track project, whatever the file is called. */
int car_project_is_masks(const char *dir);

/* 1 when `dir` looks like a car project (a mod.ini in it or in its coche/, or a masks project): the cheap question for a recent list, which asks every frame. car_project_detect judges. */
int car_project_present(const char *dir);

#endif
