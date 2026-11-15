/* port (dep56): Restomod's tournament variants. The track screen's switches (Mirror, Inverse, Rally) are chosen once,
 * before the tournament's first race, and every race of it runs them; once a race is run they are fixed. The original
 * has no such thing: its tournament clears the variants (00432974, 00433c58), as Stock and Restored still do.
 *
 * A saved tournament keeps them in a file of the port's own next to the save, NAME.VAR beside NAME.SAV (the .SAV is
 * the exe's image and stays byte for byte the original's): written at every save, read at every load. A load with no
 * such file, or a damaged one, gives the base variants and a line in the log; nothing is kept from the tournament
 * that was in memory before. */
#ifndef TNFS_TOURNAMENT_VARIANTS_H
#define TNFS_TOURNAMENT_VARIANTS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int mirror, inverse, rally;   /* 0 / 1 each */
} TournamentVariants;

enum { TOURNAMENT_VARIANTS_TRACKS = 9, TOURNAMENT_VARIANTS_PATH = 64 };

/* the tournament in memory's; the base (all off) until one is chosen or loaded */
TournamentVariants tournament_variants_get(void);
void tournament_variants_set(TournamentVariants v);
void tournament_variants_reset(void);

/* 1 once a race of the tournament is run (its races by track, 0052b2d6..: any not 0): the variants are fixed */
int tournament_variants_locked(const int16_t races[TOURNAMENT_VARIANTS_TRACKS]);

/* What the track screen's switches leave chosen, on `track`: the rows it showed take their value, the ones it did not
 * (Lost Vegas has neither mirror nor rally, the point-to-point tracks no inverse; Lost Vegas, a circuit, has it) keep `was`'s. */
TournamentVariants tournament_variants_follow(TournamentVariants was, int track, int mirror, int inverse, int rally);

/* What a race on `track` runs: Lost Vegas (8) has neither mirror nor rally, the hidden track (9) none at all.
 * The inverse is kept as chosen; the race reads it on the circuits only (reverse_track.h). */
TournamentVariants tournament_variants_on_track(TournamentVariants v, int track);

/* What a tournament's race settings take on `track` (the flag rule: `restomod` is the live mode's, read at the call).
 * Restomod: the chosen ones on the track, each where the level offers it (mirror with `unlock_m`, rally with
 * `unlock_r`, inverse from level 2); 1. Stock and Restored: the exe's tournament has none, so when the chosen ones would
 * put any on (a mode changed in a Restomod tournament) all three off, 1; else 0, the settings left as they are. The
 * chosen ones are kept either way: back in Restomod they are the tournament's again. */
int tournament_variants_settings(TournamentVariants chosen, int track, int restomod, int unlock_m, int unlock_r, int level,
                                 TournamentVariants *out);

/* The port's file of a save: "NAME.SAV" -> "NAME.VAR" (any case of the extension). 0 when `save` has no .SAV. */
int tournament_variants_path(const char *save, char *out, size_t size);

/* The file's text, and its reading: 1 when it is whole (its first line and the three keys, each 0 or 1). */
int tournament_variants_format(TournamentVariants v, char *out, size_t size);
int tournament_variants_parse(const uint8_t *text, size_t size, TournamentVariants *out);

/* The file of save `save` (the player's directory): written from the tournament in memory, 1 when written; read into
 * it (the base and a log line when missing or damaged); removed with its save. */
int tournament_variants_save(const char *save);
void tournament_variants_load(const char *save);
void tournament_variants_remove(const char *save);

#endif
