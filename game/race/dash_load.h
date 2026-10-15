/* 00450838: the race's mask bank, the dash's file and parts (masklo /
 * maskhi, or masklog / maskhig for German), loaded from 00455a5c. The
 * reads are the port's own; these are the names and what the original does
 * with the files once read. */
#ifndef TNFS_DASH_LOAD_H
#define TNFS_DASH_LOAD_H

#include "game/race/dash_art.h"
#include "game/race/race_palette.h"

#include <stdint.h>

enum {
    DASH_PATH = 80,         /* 00450838's buffer */
    DASH_COLOUR_FIRST = 0xf0, DASH_COLOUR_COUNT = 13,
    DASH_BANK_FIRST = 0xf0, DASH_BANK_COUNT = 10,
    DASH_LOAD_NO_PALETTE = -2,  /* port guard: the bank has no '!pal' (the original aborts) */
    DASH_LOAD_NO_MMB = -3       /* port guard: no .mmb (the original faults reading it) */
};

/* 004c5534: the 13 6-bit colours queued at 0xf0. */
extern const uint8_t dash_colours[DASH_COLOUR_COUNT * 3];

typedef struct {
    char bank[DASH_PATH];   /* dir + masklo.fsh / maskhi.fsh / masklog.fsh / maskhig.fsh (00476268) */
    char mmb[DASH_PATH];    /* dir + MASKLO.mmb / MASKHI.mmb (00451278) */
    char parts[DASH_PART_ROWS][DASH_PART_COLUMNS][DASH_PATH];  /* dir + masklo.b%d%d (00483e60) */
} DashFiles;

/* The names, `dir` being paths slot 17 (005246e8); `german` 0052b31c,
 * `high` 004fa0c0. Returns 0, or -1 when a name does not fit. */
int dash_load_paths(DashFiles *files, const char *dir, int16_t german, int32_t high);

/* What 00450838 does with the bank and the .mmb once read: the bank's
 * '!pal' (+0x10, made 6-bit in place when its shape is of type 0x24), the
 * bank queued for 00447098 at 0xf0 (10) and dash_colours at 0xf0 (13), the
 * shapes (004502b0, which ends with 00471180), the state words, the .mmb's
 * words. The caller then sets art->parts from its reads of files->parts.
 * Returns 0, DASH_LOAD_NO_PALETTE, DASH_ART_MISSING or DASH_LOAD_NO_MMB. */
int dash_load_setup(DashArt *art, ReplayArt *replay_art, RacePalette *pal, uint8_t *bank, uint8_t *mmb,
                    int32_t high, int16_t replay);

typedef struct {
    void *ctx;
    void (*free)(void *ctx, void *block);   /* 00475260 */
} DashFreeOps;

/* 00450a68 (watcall ()): the bank (005137f0) freed when held, then each
 * part (00513c44, row by row) and the .mmb (00513c74) whatever they hold;
 * no pointer is cleared. */
void dash_art_free(const DashFreeOps *ops, const DashArt *art);

#endif
