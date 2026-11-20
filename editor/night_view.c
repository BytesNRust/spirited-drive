#include "night_view.h"


#include <ctype.h>
#include <string.h>

/* The ratio is what the game's log says of the player's car at the node of the Race photo (200 ticks): TR5 and TR6 0.72 0.72 0.72 (the light made from their terrain: a car with its headlights
 * on takes the headlight pool's floor plus its share), measured on 2026-11-20. The other tracks log no ratio for the car there (the street lamps are placed, the lamps' pools lift it where
 * they reach): they take the same as typical for a made light until measured. */
static const NightRef REFS[] = {
    { "Rusty Springs", "tr5", 0.72f }, { "Burnt Sienna", "tr6", 0.72f }, { "Transtropolis", "tr7", 0.72f }, { "Lost Vegas", "tr4", 0.72f }, { "Coastal 1", "cl1", 0.72f }, { "City 1", "cy1", 0.72f },
};

const NightRef *night_refs(void) { return REFS; }
int night_ref_count(void) { return (int)(sizeof REFS / sizeof *REFS); }

int night_ref_find(const char *stem)
{
    for (int i = 0; stem != NULL && i < night_ref_count(); i++)
        if (strcasecmp(REFS[i].stem, stem) == 0) return i;
    return -1;
}

NightSettings night_settings_default(void)
{
    NightSettings s = { 0, 0, 100, 1, 0, 0, 0 };
    return s;
}

CarNightState night_settings_state(const NightSettings *s, int32_t police_tick)
{
    CarNightState st;
    memset(&st, 0, sizeof st);
    if (s->night) {   /* the car is lit by the light at its place: the race's ratio table, not the world's tone (the tone is the sky's and the road's) */
        const float ratio = REFS[s->ref >= 0 && s->ref < night_ref_count() ? s->ref : 0].ratio * (float)(s->light < 0 ? 0 : s->light > NIGHT_LIGHT_MAX ? NIGHT_LIGHT_MAX : s->light) / 100.0f;
        st.lit = 1;
        st.light[0] = st.light[1] = st.light[2] = ratio;
    }   /* Day: the palette as the Preview shows it (the exe's midday tint is not applied there either) */
    st.night = s->night, st.lights = s->lights, st.brake = s->brake, st.reverse = s->reverse;
    st.police = s->police, st.police_tick = police_tick;
    return st;
}

int night_settings_plain(const NightSettings *s)
{
    return !s->night && !s->reverse && !s->police;
}
