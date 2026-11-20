#include "night_view.h"

#include "formats/night_tone.h"

#include <ctype.h>
#include <string.h>

enum { TONE_ONE = 256 };   /* a channel scale is 8.8 */

static const NightRef REFS[] = {
    { "Rusty Springs", "tr5" }, { "Burnt Sienna", "tr6" }, { "Transtropolis", "tr7" }, { "Lost Vegas", "tr4" }, { "Coastal 1", "cl1" }, { "City 1", "cy1" },
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
    NightSettings s = { 0, 0, 1, 0, 0, 0 };
    return s;
}

CarNightState night_settings_state(const NightSettings *s, int32_t police_tick)
{
    CarNightState st;
    memset(&st, 0, sizeof st);
    const char *stem = REFS[s->ref >= 0 && s->ref < night_ref_count() ? s->ref : 0].stem;
    if (s->night) {
        float tone[3];
        night_tone(night_kind_of(stem), 0, tone);   /* the port's night (the lifts at their default, 0): a flat cool tone; the INFO's own tints are the exe's dawn, midday and dusk */
        st.tint = (PaletteTint){ 0x10000, (int32_t)(tone[0] * TONE_ONE + 0.5f), 0, (int32_t)(tone[1] * TONE_ONE + 0.5f), (int32_t)(tone[2] * TONE_ONE + 0.5f) };
        st.tinted = 1;
    }   /* Day: the palette as the Preview shows it (the exe's midday tint is not applied there either) */
    st.night = s->night, st.lights = s->lights, st.brake = s->brake, st.reverse = s->reverse;
    st.police = s->police, st.police_tick = police_tick;
    return st;
}

int night_settings_plain(const NightSettings *s)
{
    return !s->night && !s->reverse && !s->police;
}
