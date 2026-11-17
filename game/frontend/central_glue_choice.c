/* port (dep65): Restomod's track switches as the player's choice (central_glue_private.h). The choice lives in the mode's file
 * (variant_mirror, variant_rally, variant_reverse), read at every use, as the time of day and the weather; the globals Control
 * Central and the race read (cg_g.variant_m / variant_r, the reverse pick) are what the track in hand runs from it, worked out
 * again wherever the exe or the port clears them (Control Central's start, Options, a race, a change of race type, a track step).
 * A tournament keeps its own variants (tournament_variants.h): there the choice is neither read nor written. */
#include "game/frontend/central_glue_private.h"
#include "game/frontend/track_pick.h"
#include "game/frontend/variant_rule.h"
#include "game/sys/port_extras.h"
#include "game/sys/reverse_track.h"

static int choice_on(void) { return variant_switches_on() && cg_g.race_type != RACE_TOURNAMENT; }

int cg_reverse_offered(void) { return port_extras_load().reverse && cg_g.unlock_level >= 2; }   /* port: INVERSE is level 2's */

void cg_variant_choice_apply(void)
{
    if (!choice_on()) return;
    const PortSettings s = port_settings_load();
    const VariantPair v = variant_choice_on_track(cg_g.track, s.variant_mirror, s.variant_rally, cg_g.unlock_m, cg_g.unlock_r);
    cg_g.variant_m = v.mirror, cg_g.variant_r = v.rally;
    if (cg_g.track == 3 && track_pick_lunar()) cg_g.variant_r = 1;   /* port (dep59 Lunar L2): Lunar is Rusty's rally, whatever the switch */
    reverse_track_pick(s.variant_reverse && cg_reverse_offered());
}

/* the choice written: mirror and rally where the level offers them (a switch not offered was not shown, its choice stays as it
 * was), the reverse unless NULL */
static void choice_write(int mirror, int rally, const int *reverse)
{
    const PortSettings was = port_settings_load();
    PortSettings s = was;
    if (cg_g.unlock_m) s.variant_mirror = mirror != 0;
    if (cg_g.unlock_r) s.variant_rally = rally != 0;
    if (reverse != NULL) s.variant_reverse = *reverse != 0;
    if (s.variant_mirror == was.variant_mirror && s.variant_rally == was.variant_rally && s.variant_reverse == was.variant_reverse) return;
    if (!port_settings_save(&s)) port_log("variants: the switches could not be saved\n");
}

void cg_variant_choice_keep(int kept_m, int kept_r, const int *reverse)
{
    if (!choice_on()) return;
    const int takes = variant_choice_takes(cg_g.track);
    const int lunar = cg_g.track == 3 && track_pick_lunar();   /* port (dep59 Lunar L5): Lunar's rally is its own, not the choice */
    choice_write(takes ? cg_g.variant_m : kept_m, takes && !lunar ? cg_g.variant_r : kept_r, reverse);
}

void cg_variants_chosen(void *ctx)
{
    (void)ctx;
    if (!choice_on()) return;
    choice_write(cg_g.variant_m, cg_g.variant_r, NULL);   /* as the code left them, on any track */
    cg_variant_choice_apply();   /* and what this track runs of them (Lost Vegas: neither) */
}
