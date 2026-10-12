/* The cockpit's teardown at the race's end (00422c20). */
#include "game/race/cockpit.h"

enum { ENABLE_A = 0x5e, ENABLE_B = 0x5f, ENABLE_FIRST = 0xfa, ENABLE_LAST = 0xfd };

void cockpit_teardown(const CockpitTeardownOps *ops, Cockpit *c, CockpitPoliceState *st, RacePalette *p,
                      RaceState *race, const CockpitTeardownWords *w)
{
    if (c->bank != NULL) ops->free_bank(ops->ctx, c->bank), c->bank = NULL;
    if (st->screen_bank != NULL) ops->free_screen(ops->ctx, st->screen_bank), st->screen_bank = NULL;
    if (c->unknown_4f9744 != 0) ops->free_fade(ops->ctx), c->unknown_4f9744 = 0;
    if (p->inverse != NULL) ops->free_inverse(ops->ctx), p->inverse = NULL;  /* 00474558 */
    p->enabled[ENABLE_A] = 1, p->enabled[ENABLE_B] = 1;
    for (int i = ENABLE_FIRST; i <= ENABLE_LAST; i++) p->enabled[i] = 1;
    race->stop_notice[0] = 0, race->stop_notice[1] = 0;
    if (*w->unknown_513e60 == 1) *w->unknown_513e60 = 0;
    *w->unknown_4b91c8 = 0;
    ops->free_fmm(ops->ctx, c->fmm);
}
