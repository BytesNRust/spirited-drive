/* port: see central_banner.h. */
#include "game/frontend/central_banner.h"

#include "game/frontend/records_route.h"
#include "game/sys/port_settings.h"
#include "game/sys/text.h"

/* above the RACE item, its left edge (x 7..71, y 445..458 on the frame): about 220 px of words, which do not fit
 * between RACE and OPTIONS (x 167) */
enum { BANNER_X = 8, BANNER_Y = 420 };
static const char BANNER_TEXT[] = "CHEATS ENABLED";

int central_cheats_on(const ControlCentralState *st)
{
    enum { TURBO_BIT = 8 };
    if (!port_records_unified()) return 0;   /* the original's records mix every race: no table with cheats, nothing to warn of */
    const RaceKind kind = records_kind_with_car(
        records_race_kind(0, 0, 0, *st->explode, *st->nitro, (*st->track_flags & TURBO_BIT) != 0), *st->player_car);
    return records_has_cheats(kind);
}

void central_banner(CentralRun *r)
{
    const ControlCentralOps *o = r->ops;
    if (!o->port_banner) return;   /* the exe: nothing there */
    r->banner_shown = central_cheats_on(r->st);
    if (r->banner_shown == 0) return;
    o->plus_text(o->ctx, TEXT_RED, BANNER_TEXT, BANNER_X, BANNER_Y);
}

int central_banner_stale(const CentralRun *r)
{
    return r->ops->port_banner && central_cheats_on(r->st) != (r->banner_shown != 0);
}
