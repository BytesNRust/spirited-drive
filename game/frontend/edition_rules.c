/* port (dep59 step 4, T7): see edition_rules.h. */
#include "game/frontend/edition_rules.h"

#include "formats/stock_content.h"

#include <stddef.h>
#include <string.h>

enum { STOCK_CARS = 8, STOCK_TRACKS = 8 };   /* rx7..diablo; city..transtropolis (vegas 8 and hidden 9 are extra) */

static int listed(const char (*ids)[MOD_INI_ID_MAX + 1], int count, const char *id)
{
    for (int i = 0; i < count; i++)
        if (strcmp(ids[i], id) == 0) return 1;
    return 0;
}

static int has_car(const Edition *e, const char *id) { return listed(e->car, e->cars, id); }
static int has_track(const Edition *e, const char *id) { return listed(e->track, e->tracks, id); }

int edition_is_stock(const Edition *e)
{
    if (e == NULL) return 0;
    for (int i = 0; i < e->cars; i++)
        if (stock_car_of_id(e->car[i]) < 0) return 0;
    for (int i = 0; i < e->tracks; i++)
        if (stock_track_of_id(e->track[i]) < 0) return 0;
    for (int car = 0; car < STOCK_CARS; car++)
        if (!has_car(e, stock_car_id(car))) return 0;
    for (int track = 0; track < STOCK_TRACKS; track++)
        if (!has_track(e, stock_track_id(track))) return 0;
    return 1;
}

EditionGate edition_gate(const Edition *e)
{
    EditionGate g = { 1, 1, NULL, NULL };
    if (e == NULL) return g;
    g.network = 0, g.network_why = "Multiplayer needs the standard edition";
    if (!edition_is_stock(e)) g.tournament = 0, g.tournament_why = "Tournament needs the stock cars and tracks";
    return g;
}

const char *edition_network_why(const Edition *e, int udp_present)
{
    return udp_present ? edition_gate(e).network_why : "Multiplayer is not in this version yet";
}

Unlocks edition_unlocks(const Edition *e)
{
    Unlocks u = { 1, 1, 1, 0, 0 };
    u.hidden = has_track(e, stock_track_id(TRACK_HIDDEN));
    u.warrior = has_car(e, stock_car_id(CAR_WARRIOR));
    return u;
}

int edition_lunar_springs(const Edition *e) { return has_track(e, stock_track_id(STOCK_TRACK_LUNAR)); }

PortExtras edition_extras(const Edition *e, PortExtras x)
{
    x.warrior = x.warrior_rival = edition_unlocks(e).warrior;
    x.lunar_springs = edition_lunar_springs(e);   /* dep59 Lunar L1: lunar in tracks= lists it, whatever the switch */
    return x;
}

ExtrasExpect edition_expect(const Edition *e)
{
    const Unlocks u = edition_unlocks(e);
    return (ExtrasExpect){ u.variant_m, u.variant_r, u.warrior, u.lost_vegas };
}
