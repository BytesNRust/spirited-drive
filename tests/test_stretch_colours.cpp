// Colour by's palettes (owner feedback T7): no class is drawn in a warm, saturated colour. Red reads as an error (Limits mode paints what Build refuses
// red) and yellow or orange as a warning, so a mode's classes use greys, blues, greens, teals and violets only.
#include "stretch_colours.h"
#include "test.h"

#include <math.h>
#include <stdio.h>

// hue in degrees (0..360) and saturation (0..1) of a colour
static void hue_sat(ImU32 c, float *hue, float *sat)
{
    const float r = (float)((c >> IM_COL32_R_SHIFT) & 255) / 255.0f, g = (float)((c >> IM_COL32_G_SHIFT) & 255) / 255.0f, b = (float)((c >> IM_COL32_B_SHIFT) & 255) / 255.0f;
    const float hi = fmaxf(r, fmaxf(g, b)), lo = fminf(r, fminf(g, b)), d = hi - lo;
    *sat = hi > 0.0f ? d / hi : 0.0f;
    float h = 0.0f;
    if (d > 0.0f) h = hi == r ? fmodf((g - b) / d, 6.0f) : hi == g ? (b - r) / d + 2.0f : (r - g) / d + 4.0f;
    h *= 60.0f;
    *hue = h < 0.0f ? h + 360.0f : h;
}

static void check_no_warm()
{
    for (int mode = 0; mode < TRACE_COLOUR_MODES; ++mode) {
        for (int k = 0; k < track_trace_class_count((TraceColourBy)mode); ++k) {
            float hue, sat;
            hue_sat(stretch_colour(mode, k), &hue, &sat);
            const bool warm = sat >= 0.35f && (hue <= 70.0f || hue >= 340.0f);   // red, orange, brown, yellow
            if (warm) printf("mode %d class %d is warm: hue %.0f saturation %.2f\n", mode, k, hue, sat);
            CHECK(!warm);
        }
    }
}

// a class's colour differs from its neighbours in the same mode, so the strip reads
static void check_distinct()
{
    for (int mode = 1; mode < TRACE_COLOUR_MODES; ++mode)
        for (int k = 0; k < track_trace_class_count((TraceColourBy)mode); ++k)
            for (int j = k + 1; j < track_trace_class_count((TraceColourBy)mode); ++j) CHECK(stretch_colour(mode, k) != stretch_colour(mode, j));
}

int main()
{
    check_no_warm();
    check_distinct();
    return TEST_REPORT();
}
