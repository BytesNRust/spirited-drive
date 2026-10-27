/* port (body shine, envmap): the sphere-map uv of a reflection (known normals and lines of sight), the packing of a uv, the procedural image
 * (the horizon line is its brightest band, the sky above is brighter than the ground below, the sun spot is bright) and the level it asks for. */
#include "game/race/car_envmap.h"
#include "game/race/car_shine.h"
#include "test.h"

#include <math.h>

static const float HALF_SQRT2 = 0.70710678f;

static int near(float a, float b) { return fabsf(a - b) < 0.001f; }

/* a surface facing the viewer reflects the viewer: the centre of the sphere map */
static void test_facing_the_viewer_is_the_centre(void)
{
    const float n[3] = { 0, 0, -1 }, v[3] = { 0, 0, 1 };
    float uv[2];
    car_envmap_uv(n, v, uv);
    CHECK(near(uv[0], 0.5f) && near(uv[1], 0.5f));
}

/* a surface turned 45 degrees to the right reflects to the right: rx = 1, rz = 0, m = 2 sqrt(2) */
static void test_turned_to_the_right_maps_right_of_centre(void)
{
    const float n[3] = { HALF_SQRT2, 0, -HALF_SQRT2 }, v[3] = { 0, 0, 1 };
    float uv[2];
    car_envmap_uv(n, v, uv);
    CHECK(near(uv[0], 0.5f + 0.5f / (2.0f * sqrtf(2.0f))));   /* r = (1, 0, 0), m = 2 sqrt2: 0.6768 */
    CHECK(near(uv[1], 0.5f));
}

/* an up-facing panel seen from above at 45 degrees: the reflection points up and back, so it maps into the upper half (the sky) */
static void test_an_up_facing_panel_reflects_the_sky(void)
{
    const float n[3] = { 0, 1, 0 }, v[3] = { 0, -HALF_SQRT2, HALF_SQRT2 };
    float uv[2];
    car_envmap_uv(n, v, uv);
    CHECK(near(uv[0], 0.5f));
    CHECK(near(uv[1], 0.7310f));   /* r = (0, s, c) with s = c = 1/sqrt2: v = s / (2 sqrt(s^2 + (1 - c)^2)) * 0.5 + 0.5 */
}

/* the same panel seen from below the sky line reflects the ground: the mirror of the case above about v = 0.5 */
static void test_a_reflection_below_the_horizon_maps_below_the_centre(void)
{
    const float n[3] = { 0, 1, 0 }, v[3] = { 0, HALF_SQRT2, HALF_SQRT2 };
    float uv[2];
    car_envmap_uv(n, v, uv);
    CHECK(near(uv[1], 1.0f - 0.7310f));
    CHECK(uv[1] < 0.5f);
}

static void test_pack_and_unpack_round_trip(void)
{
    const float in[2] = { 0.25f, 0.75f };
    float out[2];
    car_envmap_unpack(car_envmap_pack(in), out);
    CHECK(fabsf(out[0] - in[0]) < 0.0001f && fabsf(out[1] - in[1]) < 0.0001f);
    const float edge[2] = { 1.0f, 0.0f };
    car_envmap_unpack(car_envmap_pack(edge), out);
    CHECK(near(out[0], 1.0f) && near(out[1], 0.0f));
}

enum { S = CAR_ENVMAP_SIZE, MID = CAR_ENVMAP_SIZE / 2 };

#define AT(x, y) (image[(y) * S + (x)])

/* additive only: nothing below the neutral byte (no tint or darkening on the body), most of the image transparent (the neutral), and the
 * streaks are light (white) where they are */
static void test_the_image_is_additive_and_mostly_transparent(void)
{
    const uint8_t *image = car_envmap_image();
    int clear = 0, lit = 0, below = 0;
    for (int i = 0; i < S * S; i++) {
        clear += image[i] == 128;
        lit += image[i] > 200;
        below += image[i] < 128;
    }
    CHECK(below == 0);
    CHECK(clear > S * S / 2);
    CHECK(lit > 0);
}

/* the horizon is a sharp, unbroken bright line through the centre: the two rows either side of the middle of the centre column are light,
 * and a few rows away it is clear again */
static void test_the_horizon_is_a_sharp_bright_line(void)
{
    const uint8_t *image = car_envmap_image();
    CHECK(AT(MID, MID - 1) > 200 && AT(MID, MID) > 200);
    CHECK(AT(MID, MID - 12) == 128 && AT(MID, MID + 12) == 128);
}

/* the sun sits upper left of the centre (about x 48, y 48 at this size): a small spot brighter than the sky round it */
static void test_the_sun_spot_is_bright(void)
{
    const uint8_t *image = car_envmap_image();
    CHECK(AT(48, 48) >= 240);
    CHECK(AT(48, 48) > AT(100, 48));
}

static void test_sampling_takes_the_texel_at_the_uv(void)
{
    const uint8_t *image = car_envmap_image();
    CHECK_EQ_INT(car_envmap_sample(image, 0.5f, 0.5f), AT(MID, MID));
    CHECK_EQ_INT(car_envmap_sample(image, 0.0f, 1.0f), AT(0, 0));
    CHECK_EQ_INT(car_envmap_sample(image, 1.0f, 0.0f), AT(S - 1, S - 1));
}

/* the neutral byte asks for the neutral level, the top for a lift toward the lightest, the bottom for a drop toward the darkest, and the
 * levels follow the bytes in order */
static void test_the_level_follows_the_byte(void)
{
    CHECK(fabsf(car_envmap_level(128, CAR_ENVMAP_BODY_GAIN) - CAR_SHINE_NEUTRAL) < 0.0001f);   /* transparent: the paint as it is */
    CHECK(fabsf(car_envmap_level(0, CAR_ENVMAP_BODY_GAIN) - CAR_SHINE_NEUTRAL) < 0.0001f);     /* additive: no darkening */
    CHECK(car_envmap_level(255, CAR_ENVMAP_BODY_GAIN) > CAR_SHINE_LEVELS - 2.5f);
    float last = -1.0f;
    for (int b = 0; b < 256; b++) {
        const float l = car_envmap_level((uint8_t)b, CAR_ENVMAP_BODY_GAIN);
        CHECK(l >= last);
        last = l;
    }
}

int main(void)
{
    test_facing_the_viewer_is_the_centre();
    test_turned_to_the_right_maps_right_of_centre();
    test_an_up_facing_panel_reflects_the_sky();
    test_a_reflection_below_the_horizon_maps_below_the_centre();
    test_pack_and_unpack_round_trip();
    test_the_image_is_additive_and_mostly_transparent();
    test_the_horizon_is_a_sharp_bright_line();
    test_the_sun_spot_is_bright();
    test_sampling_takes_the_texel_at_the_uv();
    test_the_level_follows_the_byte();
    return TEST_REPORT();
}
