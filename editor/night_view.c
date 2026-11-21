#include "night_view.h"

#include <string.h>

/* The editor's remembered light and tint (night_pref_set): the defaults of a car opened later, and what the ini keeps. */
static int g_pref_set, g_pref_light = NIGHT_LIGHT_DEFAULT;
static float g_pref_tint[3] = { NIGHT_TINT_DEFAULT, NIGHT_TINT_DEFAULT, NIGHT_TINT_DEFAULT };

static int clamp_light(int light)
{
    return light < NIGHT_LIGHT_MIN ? NIGHT_LIGHT_MIN : light > NIGHT_LIGHT_MAX ? NIGHT_LIGHT_MAX : light;
}

static float clamp_channel(float c)
{
    return !(c >= 0.0f) ? 0.0f : c > 1.0f ? 1.0f : c;   /* not a number: 0 */
}

void night_pref_set(int light, const float tint[3])
{
    g_pref_set = 1;
    g_pref_light = clamp_light(light);
    for (int i = 0; i < 3; i++) g_pref_tint[i] = clamp_channel(tint[i]);
}

int night_pref_get(int *light, float tint[3])
{
    if (!g_pref_set) return 0;
    *light = g_pref_light;
    for (int i = 0; i < 3; i++) tint[i] = g_pref_tint[i];
    return 1;
}

NightSettings night_settings_default(void)
{
    NightSettings s;
    memset(&s, 0, sizeof s);
    s.light = g_pref_set ? g_pref_light : NIGHT_LIGHT_DEFAULT;
    for (int i = 0; i < 3; i++) s.tint[i] = g_pref_set ? g_pref_tint[i] : NIGHT_TINT_DEFAULT;
    s.lights = 1;
    return s;
}

void night_settings_reset_light(NightSettings *s)
{
    s->light = NIGHT_LIGHT_DEFAULT;
}

void night_settings_reset_tint(NightSettings *s)
{
    for (int i = 0; i < 3; i++) s->tint[i] = NIGHT_TINT_DEFAULT;
}

CarNightState night_settings_state(const NightSettings *s, int32_t police_tick)
{
    CarNightState st;
    memset(&st, 0, sizeof st);
    if (s->night) {   /* the car is lit by the light at its place: this tint times the light, not the world's tone (the tone is the sky's and the road's) */
        const float percent = (float)clamp_light(s->light) / 100.0f;
        st.lit = 1;
        for (int i = 0; i < 3; i++) st.light[i] = clamp_channel(s->tint[i]) * percent;
    }   /* Day: the palette as the Preview shows it (the exe's midday tint is not applied there either) */
    st.night = s->night, st.lights = s->lights, st.brake = s->brake, st.reverse = s->reverse;
    st.police = s->police, st.police_tick = police_tick;
    return st;
}

int night_settings_plain(const NightSettings *s)
{
    return !s->night && !s->brake && !s->reverse && !s->police;   /* by day the brake and the reversing lights glow (as in the race) */
}
