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

/* one flag: it is on when expected (level or switch) or typed; a flag that is on though nobody expects it, and was
 * not expected the time before either, was typed */
static int settle_one(int cur, int expected, int *was_expected, int *typed)
{
    if (cur && !expected && !*was_expected) *typed = 1;
    *was_expected = expected;
    return expected || *typed;
}

void extras_settle(const PortExtras *e, int level, const ExtrasFlags *f, ExtrasSession *s)
{
    const int m = settle_one(*f->unlock_m != 0, level > 0 || e->mirror, &s->on_m, &s->typed_m);
    const int r = settle_one(*f->unlock_r != 0, level > 1 || e->rally, &s->on_r, &s->typed_r);
    /* Lost Vegas: the cheat flag is the switch's (the level sets the other half alone) */
    const int v = settle_one(*f->vegas_cheat != 0, e->vegas != 0, &s->on_vegas, &s->typed_vegas);
    const int w = settle_one(*f->warrior != 0, level > 0 || e->warrior, &s->on_warrior, &s->typed_warrior);
    const int x = settle_one(*f->explode != 0, e->explode != 0, &s->on_explode, &s->typed_explode);
    const int n = settle_one(*f->nitro != 0, e->nitro != 0, &s->on_nitro, &s->typed_nitro);
    const int t = settle_one((*f->track_flags & 8) != 0, e->turbo != 0, &s->on_turbo, &s->typed_turbo);
    if (*f->unlock_m != 0 && !m) *f->unlock_m = 0, *f->variant_m = 0;
    if (*f->unlock_r != 0 && !r) *f->unlock_r = 0, *f->variant_r = 0;
    *f->vegas_cheat = v;
    if (v && *f->lost_vegas == 0) *f->lost_vegas = 1;
    if (!v && level == 0) *f->lost_vegas = 0;
    *f->warrior = (int16_t)w;
    *f->explode = x;
    *f->nitro = n;
    if (t) *f->track_flags |= 8;
    else *f->track_flags &= ~8u;
    extras_reapply_port(e, f);   /* the switches that are on, as they always came back */
    if (m) *f->unlock_m = 1;
    if (r) *f->unlock_r = 1;
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
