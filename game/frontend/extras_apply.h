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

/* port: as extras_reapply, but with both switches on the two variants stay on (each unlocked), so the race can
 * run the mirror's geometry on the rally's track. A single switch is the same answer as the exe's. */
void extras_reapply_port(const PortExtras *e, const ExtrasFlags *f);

/* port: what the flags are for, kept across the visits: the switches and the unlock level say what is on, and a flag
 * that is on with neither saying so was typed (a cheat code): it stays on for the session, but is never a switch, so it
 * is not saved and turning the switch off does not take it away. `on_*` is what the last extras_settle expected from
 * the level and the switches (a flag that was expected and no longer is has been switched off); `typed_*` the cheats
 * typed. */
typedef struct {
    int32_t on_m, on_r, on_vegas, on_warrior, on_explode, on_nitro, on_turbo;
    int32_t typed_m, typed_r, typed_vegas, typed_warrior, typed_explode, typed_nitro, typed_turbo;
} ExtrasSession;

/* port: puts the flags in line with `level` (config.dat's unlock level, the live short), the switches and the cheats
 * typed in the session: on for each of those that asks, off for a switch that was on and now is not, and left alone
 * otherwise. The unlock of Mirror or Rally switched off takes its variant off too. The variants are otherwise the
 * player's own choice and are not touched, and neither are the other flags. Idempotent. */
void extras_settle(const PortExtras *e, int level, const ExtrasFlags *f, ExtrasSession *s);

/* After a cheat code has been typed into the flags: what the screen should now show. The switches the flags do
 * not own (the cameras, the Warrior as a rival, the movies, the aids, the start screen) are kept as they were. */
PortExtras extras_from_flags(const PortExtras *e, const ExtrasFlags *f);

/* port: as extras_from_flags, but a state with both variants set shows both switches, where the exe's read
 * gives the first branch and so shows the mirror alone. */
PortExtras extras_from_flags_port(const PortExtras *e, const ExtrasFlags *f);

/* After 00465ec8 with "no driving aids" on: ABS and traction control Off (1, the options screen's order On 0, Off 1,
 * N/A 2) in the cars that have them; a car without them (2) and the gearbox stay as they are. */
void extras_no_assists(int16_t *abs, int16_t *traction);

#endif
