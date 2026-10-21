/* port: see variant_rule.h. */
#include "game/frontend/variant_rule.h"

#include "game/sys/port_extras.h"

/* Rusty Springs, the setup's track 3 (00524e85 / +6), whose rally variant is a track of its own */
enum { TRACK_TR1 = 3 };
static const char TR5[4] = "TR5";

int variant_rule_combine(void)
{
    const PortExtras e = port_extras_load();
    return e.mirror != 0 && e.rally != 0;
}

int variant_rule_rally(int variant_m, int variant_r, int combine)
{
    if (variant_r == 0) return 0;
    /* both on is a state the original never reaches; only the port's rule answers differently there */
    return variant_m == 0 || combine != 0;
}

int central_track_art_base(int variant_m, int variant_r, int combine)
{
    /* the exe's own order: the mirror's ten entries come before the rally's, so both set gives 10 */
    if (variant_m != 0 && !(combine != 0 && variant_r != 0)) return 10;
    return variant_r != 0 ? 19 : 0;
}

const char *rally_track_stem(const char *in, int variant_m, int variant_r, int combine)
{
    /* 004275f8: 004fbadc on track 3 races "tr5" (004ae1e0) */
    if (variant_rule_rally(variant_m, variant_r, combine) && in[0] == 'T' && in[1] == 'R' && in[2] == '1')
        return TR5;
    return in;
}