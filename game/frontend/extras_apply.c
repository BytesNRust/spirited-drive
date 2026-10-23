/* port: the extras put back where the original clears them. */
#include "game/frontend/extras_apply.h"

/* the extras neither rule touches: the same tail for both reapply entry points */
static void extras_apply_rest(const PortExtras *e, const ExtrasFlags *f)
{
    if (e->warrior)
        *f->warrior = 1;
    if (e->vegas) {
        *f->vegas_cheat = 1;
        *f->lost_vegas = 1;
    }
    if (e->explode)
        *f->explode = 1;
    if (e->nitro)
        *f->nitro = 1;
    if (e->turbo)
        *f->track_flags |= 8;   /* 0052b306's bit 3, as central_keys.c's CENTRAL_TURBO writes it */
}

/* the switches that only mirror the flags, the tail of both read-back rules */
static PortExtras extras_rest_from_flags(PortExtras r, const ExtrasFlags *f)
{
    r.warrior = *f->warrior != 0;
    r.vegas = *f->vegas_cheat != 0;
    r.explode = *f->explode != 0;
    r.nitro = *f->nitro != 0;
    r.turbo = (*f->track_flags & 8) != 0;
    return r;
}

void extras_reapply(const PortExtras *e, const ExtrasFlags *f)
{
    /* Mirror and rally exclude each other, as in central_keys.c's cheat_codes: switching one on clears the
     * other, and only the one the switch names runs. */
    if (e->mirror) {
        *f->unlock_m = 1;
        *f->variant_m = 1;
        *f->variant_r = 0;
    } else if (e->rally) {
        *f->unlock_r = 1;
        *f->variant_r = 1;
        *f->variant_m = 0;
    }
    extras_apply_rest(e, f);
}

void extras_reapply_port(const PortExtras *e, const ExtrasFlags *f)
{
    /* port: a switch forces its variant on and never clears the other, so 004fbad8 and 004fbadc can both stay set
     * for the race to read (the mirror's geometry, the rally's art): both switches on, or one on and the other's
     * entry picked on the track screen. */
    if (e->mirror) {
        *f->unlock_m = 1;
        *f->variant_m = 1;
    }
    if (e->rally) {
        *f->unlock_r = 1;
        *f->variant_r = 1;
    }
    extras_apply_rest(e, f);
}

PortExtras extras_from_flags(const PortExtras *e, const ExtrasFlags *f)
{
    PortExtras r = *e;
    /* The variant the game has active is the switch that is on; rally is read first so that a state with both
     * flags somehow set (the original clears one when the other is typed) shows rally, as the last code won. */
    if (*f->variant_m != 0)
        r.mirror = 1, r.rally = 0;
    else if (*f->variant_r != 0)
        r.rally = 1, r.mirror = 0;
    return extras_rest_from_flags(r, f);
}

PortExtras extras_from_flags_port(const PortExtras *e, const ExtrasFlags *f)
{
    PortExtras r = *e;
    /* port: with both variants on, both switches show — the pair is a state the race can now run in, and
     * showing one of them alone would put the screen's answer out of step with the flags. */
    if (*f->variant_m != 0 && *f->variant_r != 0)
        r.mirror = 1, r.rally = 1;
    else if (*f->variant_m != 0)
        r.mirror = 1, r.rally = 0;
    else if (*f->variant_r != 0)
        r.rally = 1, r.mirror = 0;
    return extras_rest_from_flags(r, f);
}

void extras_no_assists(int16_t *abs, int16_t *traction)
{
    enum { OFF = 1, NOT_OFFERED = 2 };
    if (*abs < NOT_OFFERED) *abs = OFF;
    if (*traction < NOT_OFFERED) *traction = OFF;
}
