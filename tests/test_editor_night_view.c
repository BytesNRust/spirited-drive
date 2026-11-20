/* The car editor's night view settings (editor/night_view.h): the reference tracks, and the state each combination of switches gives. */
#include "formats/lamp_emissive.h"
#include "night_view.h"
#include "test.h"

#include <math.h>

int main(void)
{
    CHECK(night_ref_count() >= 4);
    CHECK(night_ref_find("tr5") == 0 && night_ref_find("TR5") == 0);   /* Rusty Springs is the default */
    CHECK(night_ref_find("nope") < 0 && night_ref_find(NULL) < 0);
    NightSettings s = night_settings_default();
    CHECK(!s.night && s.lights && !s.brake && !s.reverse && !s.police && s.ref == 0);
    CHECK(night_settings_plain(&s));                                       /* Day with nothing on: the Preview's own picture */
    CarNightState st = night_settings_state(&s, 0);
    CHECK(!st.tinted && car_night_lamp_state(&st) == LAMP_DAY);            /* by day the lights switch does nothing */
    s.brake = 1;
    st = night_settings_state(&s, 0);
    CHECK(car_night_lamp_state(&st) == LAMP_DAY && night_settings_plain(&s));   /* nor does the brake */
    s.reverse = 1;
    st = night_settings_state(&s, 0);
    CHECK(car_night_lamp_state(&st) == (LAMP_DAY | LAMP_REVERSING) && !night_settings_plain(&s));
    s = night_settings_default();
    s.night = 1;
    st = night_settings_state(&s, 0);
    CHECK(st.tinted && st.tint.red < 0x100 && st.tint.blue > st.tint.red);   /* the cool night tone: blue above red, all under 1 */
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
    s.ref = 3;   /* Lost Vegas */
    NightSettings c = s;
    c.ref = 5;   /* City 1 */
    CHECK(night_settings_state(&s, 0).tint.red < 0x100 && night_settings_state(&c, 0).tint.red < 0x100);
    return TEST_REPORT();
}
