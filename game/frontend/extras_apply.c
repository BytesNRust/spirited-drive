/* port: the extras put back where the original clears them. */
#include "game/frontend/extras_apply.h"

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

PortExtras extras_from_flags(const PortExtras *e, const ExtrasFlags *f)
{
    PortExtras r = *e;
    /* The variant the game has active is the switch that is on; rally is read first so that a state with both
     * flags somehow set (the original clears one when the other is typed) shows rally, as the last code won. */
    if (*f->variant_m != 0)
        r.mirror = 1, r.rally = 0;
    else if (*f->variant_r != 0)
        r.rally = 1, r.mirror = 0;
    r.warrior = *f->warrior != 0;
    r.vegas = *f->vegas_cheat != 0;
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
