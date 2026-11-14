/* port: the extras put back where the original clears them. */
#include "game/frontend/extras_apply.h"

void extras_reapply(const PortExtras *e, const ExtrasFlags *f)
{
    /* mirror, rally and Lost Vegas are the unlock level's (extras_settle), as in the exe: no switch puts them back */
    if (e->warrior)
        *f->warrior = 1;
    if (e->explode)
        *f->explode = 1;
    if (e->nitro)
        *f->nitro = 1;
    if (e->turbo)
        *f->track_flags |= 8;   /* 0052b306's bit 3, as central_keys.c's CENTRAL_TURBO writes it */
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
    const int m = settle_one(*f->unlock_m != 0, level > 0, &s->on_m, &s->typed_m);
    const int r = settle_one(*f->unlock_r != 0, level > 1, &s->on_r, &s->typed_r);
    /* Lost Vegas: the cheat flag is only ever typed (CASINOVILLE); the level sets the other half alone */
    const int v = settle_one(*f->vegas_cheat != 0, 0, &s->on_vegas, &s->typed_vegas);
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
    extras_reapply(e, f);   /* the switches that are on, as they always came back */
    if (m) *f->unlock_m = 1;
    if (r) *f->unlock_r = 1;
}

PortExtras extras_from_flags(const PortExtras *e, const ExtrasFlags *f)
{
    PortExtras r = *e;
    r.warrior = *f->warrior != 0;
    r.explode = *f->explode != 0;
    r.nitro = *f->nitro != 0;
    r.turbo = (*f->track_flags & 8) != 0;
    return r;
}

void extras_no_assists(int16_t *abs, int16_t *traction)
{
    enum { OFF = 1, NOT_OFFERED = 2 };
    if (*abs < NOT_OFFERED) *abs = OFF;
    if (*traction < NOT_OFFERED) *traction = OFF;
}
