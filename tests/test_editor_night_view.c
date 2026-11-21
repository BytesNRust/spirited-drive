/* The car editor's night view settings (editor/night_view.h): the light and tint, their Reset buttons and memory, and the state each combination of switches gives. */
#include "formats/lamp_emissive.h"
#include "night_view.h"
#include "test.h"

#include <math.h>

int main(void)
{
    NightSettings s = night_settings_default();
    CHECK(!s.night && s.lights && !s.brake && !s.reverse && !s.police && s.light == 100);
    CHECK(s.tint[0] == NIGHT_TINT_DEFAULT && s.tint[1] == NIGHT_TINT_DEFAULT && s.tint[2] == NIGHT_TINT_DEFAULT);   /* the neutral grey: Rusty Springs' light at the car, as the view has always been */
    CHECK(night_settings_plain(&s));                                       /* Day with nothing on: the Preview's own picture */
    CarNightState st = night_settings_state(&s, 0);
    CHECK(!st.tinted && car_night_lamp_state(&st) == LAMP_DAY);            /* by day the lights switch does nothing */
    s.brake = 1;
    st = night_settings_state(&s, 0);
    CHECK(car_night_lamp_state(&st) == LAMP_DAY && st.brake && !night_settings_plain(&s));   /* the brake does (L1: by day the race lights it too) */
    s.reverse = 1;
    st = night_settings_state(&s, 0);
    CHECK(car_night_lamp_state(&st) == (LAMP_DAY | LAMP_REVERSING) && !night_settings_plain(&s));
    s = night_settings_default();
    s.night = 1;
    st = night_settings_state(&s, 0);
    CHECK(!st.tinted && st.lit && st.light[0] == st.light[1] && st.light[0] > 0.7f && st.light[0] < 0.74f);   /* the default picture: 0.72 at 100 % */
    /* the car at night is its palette times the tint times the light: each channel is the tint's times the percent */
    s.tint[0] = 0.67f, s.tint[1] = 0.43f, s.tint[2] = 0.14f;
    st = night_settings_state(&s, 0);
    CHECK(st.light[0] > 0.66f && st.light[0] < 0.68f && st.light[1] > 0.42f && st.light[1] < 0.44f && st.light[2] > 0.13f && st.light[2] < 0.15f);
    s.light = 50;
    st = night_settings_state(&s, 0);
    CHECK(st.light[0] > 0.33f && st.light[0] < 0.34f && st.light[2] > 0.06f && st.light[2] < 0.08f);   /* the slider is a percent of it */
    s.light = 250;
    st = night_settings_state(&s, 0);
    CHECK(st.light[0] > 1.6f && st.light[0] < 1.7f);   /* up to 250 % */
    /* Reset puts each back: the light to 100 %, the tint to the grey */
    night_settings_reset_light(&s);
    CHECK(s.light == 100 && s.tint[1] == 0.43f);
    night_settings_reset_tint(&s);
    CHECK(s.tint[0] == NIGHT_TINT_DEFAULT && s.tint[1] == NIGHT_TINT_DEFAULT && s.tint[2] == NIGHT_TINT_DEFAULT && s.light == 100);
    s.light = 100;
    CHECK(car_night_lamp_state(&st) == LAMP_LOW && !night_settings_plain(&s));
    s.lights = 0;
    st = night_settings_state(&s, 0);
    CHECK(car_night_lamp_state(&st) == LAMP_DAY);                           /* night, lights off: nothing lit */
    s.brake = 1;
    st = night_settings_state(&s, 0);
    CHECK(car_night_lamp_state(&st) == LAMP_BRAKING);
    s.police = 1;
    st = night_settings_state(&s, 30);
    CHECK(st.police && st.police_tick == 30);
    /* the editor's memory (the ini): none at first; once set, the defaults of a car opened later; out-of-range values are clamped */
    int light = 0;
    float tint[3];
    CHECK(!night_pref_get(&light, tint));
    const float want[3] = { 0.2f, 1.5f, -3.0f };
    night_pref_set(500, want);
    CHECK(night_pref_get(&light, tint) && light == NIGHT_LIGHT_MAX && tint[0] == 0.2f && tint[1] == 1.0f && tint[2] == 0.0f);
    NightSettings fresh = night_settings_default();
    CHECK(fresh.light == NIGHT_LIGHT_MAX && fresh.tint[0] == 0.2f && fresh.tint[1] == 1.0f && !fresh.night && fresh.lights);
    const float grey[3] = { NIGHT_TINT_DEFAULT, NIGHT_TINT_DEFAULT, NIGHT_TINT_DEFAULT };
    night_pref_set(1, grey);
    CHECK(night_pref_get(&light, tint) && light == NIGHT_LIGHT_MIN);
    return TEST_REPORT();
}
