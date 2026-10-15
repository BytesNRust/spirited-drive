/* `--drive`'s race palette, built as 00455a5c builds it from the parts that
 * are ported (see STATE.md, Race palette): the track's colours at 0x90, the
 * cars' (004572e0), the colour search's entries (0041f818's first part),
 * each car's banks (004574ec) and the track's banks (00446fd0) mapped onto
 * it in place, then the queued banks (00447098); the mask bank's colours (0xf0.., 00450838) and
 * bank when `hud` has its files. Not yet: the dashboard (0..0x2f,
 * 0041f988), the tint of the time of day (004471ac, a neutral one here) and
 * the cars' mesh colours (the flat sink still matches rgb itself). */
#ifndef TNFS_DRIVE_PALETTE_H
#define TNFS_DRIVE_PALETTE_H

#include "formats/chunk.h"
#include "game/race/car_model.h"
#include "game/race/race_palette.h"
#include "game/race/race_state.h"
#include "viewer/drive_hud.h"

#include <stdint.h>

/* fam: the track's FAM; cars[i]: slot i's model as its loader left it
 * (car_load_setup; NULL: none);
 * carpal: CARFAMS/CARPAL.FSH (NULL: none; races of type 2 and up need it).
 * The banks are remapped in place in the files they were read from (owned
 * by the caller, writable), so this runs once per load. rgb: the screen
 * palette, 8 bits a channel. Returns 0 when the track has no terrain bank
 * with a '!pal' (nothing is remapped then). */
/* late (NULL: none): run where 00456a7c queues its bank (before the HUD's
 * start), after_banks as 00447098's last call (00456948), once the queued
 * banks are mapped. */
typedef struct {
    void *ctx;
    void (*run)(void *ctx, RacePalette *p);
    void (*after_banks)(void *ctx, RacePalette *p);
} DrivePaletteLate;

int drive_palette_build(const RaceState *race, const Chunk *fam, CarLoad *const cars[RACE_CARS], uint8_t *carpal,
                        DriveHud *hud, const DrivePaletteLate *late, uint8_t rgb[768]);

#endif
