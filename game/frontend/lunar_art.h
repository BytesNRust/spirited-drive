/* port (dep59, Lunar Springs' own entry, notes/dep59-lunar-entry-plan.md L4): the track detail's art for Lunar Springs, which the CD does
 * not have (no MOONSEL / MOONBST), made from Rusty Springs' (RUSTSEL / RUSTBST, decompressed SHPI banks) as in the owner's approved
 * design (notes/captures/lunar_sel_mock.png, lunar_bst_mock.png):
 *   - the picture in greys, but for the red the screen reads by (the map's racing line, Best Times' column heads), kept in boxes;
 *   - "RUSTY" of the title gone (its box filled in from its border, with the texture's grain) and "LUNAR" written with the CD's own
 *     letters and their shadow: L from Lost Vegas' title (LOSTSEL), u from "RUSTY", n and r from "SPRINGS", a from "RACEWAY";
 *   - on the track detail, the baked "Time of day" line gone (Lunar Springs is always by day), as Lost Vegas' art has none;
 *   - the bank's other pictures (DONE, Best times, the arrows, lit and dim) in greys too, their lit yellow kept;
 *   - the palette: entry 0 and the pointer's 253..255 as they were, the colours kept, then a ramp of greys.
 * The title's letters are found, not measured: the same code serves the German art (GART), whose title is the same.
 * lunar_art_make reads no file and keeps nothing: lunar_art_glue.c reads the CD's banks and keeps the result (art_cache). */
#ifndef TNFS_LUNAR_ART_H
#define TNFS_LUNAR_ART_H

#include <stddef.h>
#include <stdint.h>

typedef enum { LUNAR_ART_SELECT = 0, LUNAR_ART_BEST = 1 } LunarArtKind;

enum {
    LUNAR_ART_VERSION = 1,   /* the cache key's: raised by hand when what it makes changes */
    LUNAR_ART_W = 640, LUNAR_ART_H = 480,
    LUNAR_ART_KEPT_MAX = 64   /* colours kept (yellow and red); the rest of the palette is the ramp of greys */
};

/* Rusty's bank `rust` (RUSTSEL for LUNAR_ART_SELECT, RUSTBST for LUNAR_ART_BEST, decompressed) and Lost Vegas' LOSTSEL (`lost`, for the
 * L) made into Lunar Springs' bank, a SHPI in `*out` (malloc'd, `*out_size` bytes) with the same entries. 0 with a reason in `why`
 * (`why_size` bytes) when either is not the bank it should be (no 640 x 480 bgnd, no palette, a title whose letters are not found). */
int lunar_art_make(const uint8_t *rust, size_t rust_size, const uint8_t *lost, size_t lost_size, LunarArtKind kind, uint8_t **out,
                   size_t *out_size, char *why, size_t why_size);

/* (lunar_art_bank.c) Lunar Springs' bank for the track detail or Best Times in the game's language, made from the CD's (its art folder's
 * tracksel/RUSTSEL.QFS or RUSTBST.QFS and LOSTSEL.QFS) or read back from the player's cache (art_cache.h, keyed by the two banks' bytes and
 * LUNAR_ART_VERSION), put as a bank (ea_bank_put); 0 when it cannot be made (logged), and the caller shows Rusty's. */
int32_t lunar_art_bank(LunarArtKind kind);
/* (lunar_art_glue.c) the same bank's bytes (malloc'd, `*size` of them; NULL when it cannot be made), its cache entry's name in
 * `name`: lunar_art_bank's work */
uint8_t *lunar_art_cached(LunarArtKind kind, size_t *size, char *name, size_t name_size);

/* The title's letters on a grey picture (w x h, rows LUNAR_TITLE_Y0..Y1): the runs of columns with a letter's white in them, left to
 * right, at most `max`; how many. Exposed for the tests. */
enum { LUNAR_TITLE_Y0 = 12, LUNAR_TITLE_Y1 = 52, LUNAR_LETTER_WHITE = 200 };
typedef struct {
    int x0, x1;   /* first and last column, inclusive */
} LunarRun;
int lunar_title_runs(const uint8_t *grey, int w, int h, LunarRun *runs, int max);

/* `grey`'s box (x0, y0)-(x1, y1), its border kept, its inside rebuilt from the border (Laplace's: the mean of its neighbours, `passes`
 * times). Exposed for the tests. */
void lunar_fill(uint8_t *grey, int w, int x0, int y0, int x1, int y1, int passes);

#endif
