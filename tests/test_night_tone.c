/* The night's flat tint (formats/night_tone.h): the tone of a track by kind and lift, the ceilings drive_light holds the light under, the kind of a stem. */
#include "formats/night_tone.h"
#include "test.h"

#include <math.h>

static int near(float a, float b) { return fabsf(a - b) < 1e-5f; }

int main(void)
{
    CHECK(night_kind_of("CY1") == NIGHT_TONE_CITY && night_kind_of("cy3") == NIGHT_TONE_CITY);
    CHECK(night_kind_of("TR4") == NIGHT_TONE_VEGAS && night_kind_of("tr4") == NIGHT_TONE_VEGAS);
    CHECK(night_kind_of("tr5") == NIGHT_TONE_OTHER && night_kind_of("CL1") == NIGHT_TONE_OTHER && night_kind_of(NULL) == NIGHT_TONE_OTHER);
    float t[3];
    night_tone(NIGHT_TONE_OTHER, 0, t);
    CHECK(near(t[0], 0.40f) && near(t[1], 0.44f) && near(t[2], 0.52f));   /* the cool tone, whatever the lift */
    night_tone(NIGHT_TONE_OTHER, 1, t);
    CHECK(near(t[0], 0.40f));
    night_tone(NIGHT_TONE_CITY, 1, t);
    CHECK(near(t[0], 0.40f) && near(t[2], 0.52f));                         /* lifted: the full tone */
    night_tone(NIGHT_TONE_CITY, 0, t);
    CHECK(near(t[0], 0.40f * 0.35f / 0.42f) && near(t[1], 0.44f * 0.35f / 0.42f));
    night_tone(NIGHT_TONE_VEGAS, 0, t);
    CHECK(near(t[2], 0.52f * 0.35f / 0.80f));
    CHECK(near(night_ceiling(NIGHT_TONE_CITY, 0, 9), 0.35f) && near(night_ceiling(NIGHT_TONE_CITY, 1, 9), 0.42f));
    CHECK(near(night_ceiling(NIGHT_TONE_VEGAS, 0, 9), 0.35f) && near(night_ceiling(NIGHT_TONE_VEGAS, 1, 9), 0.80f));
    CHECK(near(night_ceiling(NIGHT_TONE_OTHER, 1, 0.5f), 0.5f));
    return TEST_REPORT();
}
