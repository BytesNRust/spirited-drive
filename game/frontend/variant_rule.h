/* port: which of the two variants (004fbad8 the mirror, 004fbadc the rally) a shared rule reads when the player
 * has turned both on in EXTRAS. The original never has both at once — its screen and its cheats switch one off
 * as the other is turned on — so every one of these answers is the exe's until `combine` says otherwise, where
 * the rally is the one that answers (its art, its track), the mirror staying in charge of the geometry. */
#ifndef TNFS_VARIANT_RULE_H
#define TNFS_VARIANT_RULE_H

#include <stddef.h>

/* 1 when the player has left EXTRAS' Mirror and Rally both on, the one state the original never has: its screen and
 * its cheats switch one off as the other is turned on. Every rule below asks for this alongside the two flags, so
 * the exe's answer stays in reach by passing 0 — and by passing 0 for a state that cannot combine anyway. */
int variant_rule_combine(void);

/* 1 when the rally is the variant a rule should follow: on its own when variant_r is set, and over the mirror
 * when both are and `combine` is set. `combine` 0 is the exe, where both never happen. */
int variant_rule_rally(int variant_m, int variant_r, int combine);

/* Control Central's track art: the entry of central_draw.c's TRACK_NAMES the track's picture comes from — 0 the
 * plain art, 10 the mirror's ("citym" and its seven), 19 the rally's ("cityr" and its eight). */
int central_track_art_base(int variant_m, int variant_r, int combine);

/* The stem a race runs on, `in` in capitals as 004275f8 builds it: on Rusty Springs (track 3) with the rally on
 * it is "tr5", whose art and layout are Rusty's rally variant; every other track keeps its own stem. Returns
 * `in` when there is no remap. */
const char *rally_track_stem(const char *in, int variant_m, int variant_r, int combine);

#endif