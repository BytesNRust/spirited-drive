#include "night_view.h"


#include <ctype.h>
#include <string.h>

/* The ratios are what the game's log says of the player's car at the node of the Race photo (200 ticks; "lamps: slot 0 node N ON: ... ratio r g b", night, light_effects), measured on
 * 2026-11-20 with the 911 on each track: TR5 (node 5) and TR6, CL1 (node 20) 0.72 0.72 0.72; TR4 0.65 0.64 0.61; TR7 0.67 0.43 0.14 (lit by coloured lamps); CY1 0.83 0.82 0.78. */
static const NightRef REFS[] = {
    { "Rusty Springs", "tr5", { 0.72f, 0.72f, 0.72f } }, { "Burnt Sienna", "tr6", { 0.72f, 0.72f, 0.72f } }, { "Transtropolis", "tr7", { 0.67f, 0.43f, 0.14f } },
    { "Lost Vegas", "tr4", { 0.65f, 0.64f, 0.61f } },    { "Coastal 1", "cl1", { 0.72f, 0.72f, 0.72f } },    { "City 1", "cy1", { 0.83f, 0.82f, 0.78f } },
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
        const NightRef *ref = &REFS[s->ref >= 0 && s->ref < night_ref_count() ? s->ref : 0];
        const float percent = (float)(s->light < 0 ? 0 : s->light > NIGHT_LIGHT_MAX ? NIGHT_LIGHT_MAX : s->light) / 100.0f;
        st.lit = 1;
        for (int i = 0; i < 3; i++) st.light[i] = ref->ratio[i] * percent;
    }   /* Day: the palette as the Preview shows it (the exe's midday tint is not applied there either) */
    st.night = s->night, st.lights = s->lights, st.brake = s->brake, st.reverse = s->reverse;
    st.police = s->police, st.police_tick = police_tick;
    return st;
}

int night_settings_plain(const NightSettings *s)
{
    return !s->night && !s->reverse && !s->police;
}
