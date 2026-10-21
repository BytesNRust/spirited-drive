/* port: the extras put back where the original clears them, and read back out of the same flags so the EXTRAS
 * screen shows what a typed cheat code did. Pure over the flags: no globals of its own. */
#ifndef TNFS_EXTRAS_APPLY_H
#define TNFS_EXTRAS_APPLY_H

#include <stdint.h>

#include "game/sys/port_extras.h"

/* The original's flags, by their addresses (central.h names each one). */
typedef struct {
    int32_t *unlock_m;         /* 004baf0c: the mirror variant is unlocked */
    int32_t *unlock_r;         /* 004baf10: the rally variant is unlocked */
    int32_t *variant_m;        /* 004fbad8: the track's "m" art and layout, the active one */
    int32_t *variant_r;        /* 004fbadc: the "r" one */
    int32_t *explode;          /* 004c5cd4 */
    int32_t *nitro;            /* 004faa50 */
    int32_t *vegas_cheat;      /* 004baf1c: CASINOVILLE; its other half is lost_vegas */
    int32_t *lost_vegas;       /* 004baf18 */
    int16_t *warrior;          /* 004c5cc4: a word in the original (00432e6b), as central.h has it */
    uint32_t *track_flags;     /* 0052b306: bit 3 Turbo, bit 2 No Mercy. The exe reads it as a byte, a word and a
                                * dword in different places (00434f64 byte, 00435024 word, 00435f8c dword); central.h
                                * has always taken it as the dword, and this follows it. */
} ExtrasFlags;

/* After every place the original clears the flags: 00432ff4 (central_run's start), the tournament's start,
 * 00432974 and 00465ec8. Mirror and rally exclude each other as in cheat_codes, and only the active one runs. */
void extras_reapply(const PortExtras *e, const ExtrasFlags *f);

/* After a cheat code has been typed into the flags: what the screen should now show. The switches the flags do
 * not own (the cameras, the Warrior as a rival, the movies, the aids, the start screen) are kept as they were. */
PortExtras extras_from_flags(const PortExtras *e, const ExtrasFlags *f);

/* After 00465ec8 with "no driving aids" on: ABS and traction control Off (1, the options screen's order On 0, Off 1,
 * N/A 2) in the cars that have them; a car without them (2) and the gearbox stay as they are. */
void extras_no_assists(int16_t *abs, int16_t *traction);

#endif