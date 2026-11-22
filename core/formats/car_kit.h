/* A car kit's flat sources and its build (dep85): the CFM, PBS and PDN made from the kit's files alone, no donor and no binary to start
 * from, and a car's binaries laid out as those files. In memory: the studio reads and writes the folders (car_kit_paths.h).
 *
 * car_kit_build: each bank from its flat files (shpi_from_flat), each mesh from its the 3D editor OBJ and tables by the raw builder with those
 * banks as the car's own (car_import, CarImportIn.bank), the CFM from the four (cfm_build); the PBS from specs.tsv and the PDN from
 * pdn.tsv on a zeroed record (their fields cover every byte), sealed and packed as QFS as `pack` writes them. A kit with no specs.tsv
 * (no pdn.tsv) makes no PBS (PDN). car_kit_write is its inverse: what a kit of the car holds, so the build of it gives the car again. */
#ifndef TNFS_CAR_KIT_H
#define TNFS_CAR_KIT_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum { CAR_KIT_NEAR = 0, CAR_KIT_FAR = 1, CAR_KIT_LEVELS = 2 };

/* the files of each folder, named within it */
typedef struct {
    FileSet mesh[CAR_KIT_LEVELS];   /* model/near/mesh, model/far/mesh */
    FileSet bank[CAR_KIT_LEVELS];   /* model/near/bank, model/far/bank */
    FileSet specs;                  /* specs.tsv, or empty */
    FileSet pdn;                    /* pdn.tsv, or empty */
} CarKitFlat;

typedef struct {
    uint8_t *cfm, *pbs, *pdn;       /* malloc'd; pbs, pdn NULL when the kit has no specs.tsv, no pdn.tsv */
    size_t cfm_len, pbs_len, pdn_len;
    ImportStats bank_stats[CAR_KIT_LEVELS];   /* pixels moved to the nearest palette colour (an RGB texture's) */
} CarKitBuilt;

void car_kit_flat_init(CarKitFlat *flat);
void car_kit_flat_free(CarKitFlat *flat);
void car_kit_built_free(CarKitBuilt *built);

/* the car's files from the kit's sources; 0 with the folder and file in `err` */
int car_kit_build(const CarKitFlat *in, CarKitBuilt *out, ExchangeError *err);

/* the kit's sources of a car: its CFM, and its PBS and PDN files (QFS or raw), each NULL when it has none */
int car_kit_write(const uint8_t *cfm, size_t cfm_len, const uint8_t *pbs, size_t pbs_len, const uint8_t *pdn, size_t pdn_len, CarKitFlat *out,
                  ExchangeError *err);

/* README.txt of a new kit: which folders a user edits, and that build/ is disposable */
const char *car_kit_readme(void);

#endif
