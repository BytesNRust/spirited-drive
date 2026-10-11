/* The car's model as 00457d40 loads it: the CFM's two detail levels (ORIP
 * and SHPI each), their palettes, the parts, wheels and lights the race
 * draws with. The file reads (00451278 the CFM, 0048d3e8 the PDN) are the
 * caller's; car_load_path / car_load_pdn_path name the files exactly as
 * the original does and car_load_setup does the rest on the CFM read. */
#ifndef TNFS_CAR_MODEL_H
#define TNFS_CAR_MODEL_H

#include "core/formats/chunk.h"
#include "core/formats/fsh.h"
#include "core/formats/mesh.h"
#include "game/gfx/object3d.h"
#include "game/race/car.h"
#include "game/race/race_palette.h"
#include "game/race/race_state.h"

#include <stddef.h>

enum {
    CAR_LOAD_LEVELS = 2,   /* high (CFM children 0, 1), low (2, 3) */
    CAR_LOAD_PARTS = 4,    /* inside, outside, surface, convex */
    CAR_LOAD_PATH = 80     /* the original's stack buffers */
};

typedef enum {
    CAR_LOAD_OK = 0,
    CAR_LOAD_FILE,         /* the CFM's chunks do not open */
    CAR_LOAD_PALETTE,      /* no '!PAL' or '!xxx' shape: the original aborts (004732dd) */
    CAR_LOAD_MEMORY,
    CAR_LOAD_HANGS         /* a part, tyre ref, signal or light missing: the original loops for ever */
} CarLoadResult;

/* The pointers 00457d40 keeps in the car record, as the port holds them. */
typedef struct {
    Chunk file;                         /* +0x792 */
    Mesh mesh[CAR_LOAD_LEVELS];        /* +0x52a, +0x662 */
    Fsh bank[CAR_LOAD_LEVELS];         /* +0x51e, +0x656 */
    int has_bank[CAR_LOAD_LEVELS];
    uint8_t *palette[CAR_LOAD_LEVELS]; /* +0x522, +0x65a: the '!PAL' colours */
    uint8_t *own;                       /* +0x526: the '!xxx' colours, cars not flagged 8 */
    Object3D object[CAR_LOAD_LEVELS];  /* +0x52e, +0x666 */
    Object3DList parts[CAR_LOAD_PARTS]; /* +0x5c2, +0x5ce, +0x5da, +0x5e6 */
} CarLoad;

/* 0045762c: the model's size from the high-detail object's points (car
 * +0x52e's +0x70, x y z words), `count` of them (the ORIP's +0x14 at car
 * +0x52a): each axis from the least to the most, both starting at 0, x
 * 0x200 (wraps) into model_width (x), model_height (y), model_length (z). */
void car_model_size(Car *car, const int32_t (*points)[3], int32_t count);

/* 00457d40's file name: `dir` (paths slot 13, 005245a8) + the slot's model
 * (models[car->index]): below 100 traffic (004c56a0), 100..199 'm' + a
 * racer (004c5660, +0x48a = model - 100), 200..399 a racer (+0x48a =
 * model - 200), 400 and up "CopMust" whatever the model, which loses 400
 * in models[]; then ".cfm". `stem` gets the name alone (the PDN's). Returns
 * 0 for a model past the tables (the original reads on into other tables). */
int car_load_path(Car *car, RaceState *race, const char *dir, char path[CAR_LOAD_PATH], char stem[16]);

/* The rest of 00457d40 on the CFM read into `file` (its palettes are
 * changed in place). `match` NULL leaves the texture colours unmatched
 * (0048d798 runs 00490cc0 on the palette of the moment). */
CarLoadResult car_load_setup(Car *car, RaceState *race, RacePalette *pal, const PaletteMatch *match, CarLoad *m,
                               uint8_t *file, size_t size);

/* 00457d40's tail: with 004c580c 0 (always), dir + stem + ".PDN" is the
 * body file read into 00517580[slot]. */
void car_load_pdn_path(const char *dir, const char *stem, char path[CAR_LOAD_PATH]);

void car_load_free(CarLoad *m);

#endif
