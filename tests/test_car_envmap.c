/* port (body shine, envmap): the sphere-map uv of a reflection (known normals, lines of sight and the world's up), the packing of a uv, the procedural
 * image (a sky above the neutral byte, the ground below it, the sun the brightest texel), the shade and lift of a level, and the PNG override
 * (TNFS_CAR_ENVMAP) with its fallback. */
#include "formats/png.h"
#include "game/race/car_envmap.h"
#include "game/race/car_shine.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float HALF_SQRT2 = 0.70710678f;
static const float UP[3] = { 0, 1, 0 };
static const float VIEW[3] = { 0, 0, 1 };   /* the eye looks along +z: a surface facing the viewer has its normal at -z */

/* the camera's sun direction, toward the viewer (the sphere map's frame: the camera's z is away from it), as car_envmap.c's SUN */
static const float SUN_CAMERA[3] = { -0.3478f, 0.7951f, -0.4969f };

static int near(float a, float b) { return fabsf(a - b) < 0.001f; }

/* the uv of the reflection `r` (unit, camera space) of the eye's line of sight, the normal being the bisector of the view and the reflection */
static void uv_of_reflection(const float r[3], float uv[2])
{
    float n[3] = { VIEW[0] - r[0], VIEW[1] - r[1], VIEW[2] - r[2] };
    const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    for (int k = 0; k < 3; k++) n[k] /= len;
    car_envmap_uv(n, VIEW, UP, uv);
}

/* a unit reflection at the camera-space elevation `e` (the world's up) and an azimuth */
static void reflection_at(float e, float azimuth, float r[3])
{
    const float c = sqrtf(1.0f - e * e);
    r[0] = c * cosf(azimuth), r[1] = e, r[2] = c * sinf(azimuth);
}

/* a surface facing the viewer reflects the viewer: the centre of the sphere map */
static void test_facing_the_viewer_is_the_centre(void)
{
    const float n[3] = { 0, 0, -1 };
    float uv[2];
    car_envmap_uv(n, VIEW, UP, uv);
    CHECK(near(uv[0], 0.5f) && near(uv[1], 0.5f));
}

/* a surface turned 45 degrees to the right reflects to the right: r = (1, 0, 0), m = 2 sqrt2, u = x/m + 0.5 */
static void test_turned_to_the_right_maps_right_of_centre(void)
{
    const float n[3] = { HALF_SQRT2, 0, -HALF_SQRT2 };
    float uv[2];
    car_envmap_uv(n, VIEW, UP, uv);
    CHECK(near(uv[0], 0.5f + 1.0f / (2.0f * sqrtf(2.0f))));   /* 0.8536 */
    CHECK(near(uv[1], 0.5f));
}

/* an up-facing panel seen from above at 45 degrees: the reflection points up and back, so it maps into the upper half (the sky):
 * r = (0, s, c) with s = c = 1/sqrt2, m = 2 sqrt(s^2 + (1 - c)^2) and v = s / m + 0.5 */
static void test_an_up_facing_panel_reflects_the_sky(void)
{
    const float n[3] = { 0, 1, 0 }, v[3] = { 0, -HALF_SQRT2, HALF_SQRT2 };
    float uv[2];
    car_envmap_uv(n, v, UP, uv);
    CHECK(near(uv[0], 0.5f));
    CHECK(near(uv[1], 0.9619f));
}

/* the same panel seen from below the sky line reflects the ground: the mirror of the case above about v = 0.5 */
static void test_a_reflection_below_the_horizon_maps_below_the_centre(void)
{
    const float n[3] = { 0, 1, 0 }, v[3] = { 0, HALF_SQRT2, HALF_SQRT2 };
    float uv[2];
    car_envmap_uv(n, v, UP, uv);
    CHECK(near(uv[1], 1.0f - 0.9619f));
    CHECK(uv[1] < 0.5f);
}

/* the sphere map follows the world's up: a camera rolled about its own axis (every vector turned together, the world's up with them) maps
 * the same reflection to the same uv, so the sky does not tilt with the camera */
static void test_a_camera_roll_does_not_move_the_sky(void)
{
    const float n[3] = { 0.36f, 0.48f, -0.8f }, v[3] = { 0.1f, -0.2f, 0.9747f };
    float before[2];
    car_envmap_uv(n, v, UP, before);
    const float a = 0.7f, c = cosf(a), s = sinf(a);
    const float rn[3] = { c * n[0] - s * n[1], s * n[0] + c * n[1], n[2] };
    const float rv[3] = { c * v[0] - s * v[1], s * v[0] + c * v[1], v[2] };
    const float rup[3] = { -s, c, 0 };   /* the world's up in the rolled camera */
    float after[2];
    car_envmap_uv(rn, rv, rup, after);
    CHECK(fabsf(before[0] - after[0]) < 1e-3f && fabsf(before[1] - after[1]) < 1e-3f);
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

enum { S = CAR_ENVMAP_SIZE };

/* the sky (above the horizon) is above the neutral byte, the ground (below it) is below, at every azimuth */
static void test_the_sky_is_above_neutral_and_the_ground_below(void)
{
    const uint8_t *image = car_envmap_image();
    for (int i = 0; i < 8; i++) {
        float r[3], uv[2];
        reflection_at(0.5f, i * 0.785f, r);
        uv_of_reflection(r, uv);
        CHECK(car_envmap_sample(image, uv[0], uv[1]) > 128);
        reflection_at(-0.5f, i * 0.785f, r);
        uv_of_reflection(r, uv);
        CHECK(car_envmap_sample(image, uv[0], uv[1]) < 128);
    }
}

/* the sky brightens toward the zenith */
static void test_the_sky_is_brighter_toward_the_zenith(void)
{
    const uint8_t *image = car_envmap_image();
    float low[3], high[3], uv[2];
    reflection_at(0.2f, 0.3f, low);
    reflection_at(0.9f, 0.3f, high);
    uv_of_reflection(low, uv);
    const uint8_t a = car_envmap_sample(image, uv[0], uv[1]);
    uv_of_reflection(high, uv);
    const uint8_t b = car_envmap_sample(image, uv[0], uv[1]);
    CHECK(b > a);
}

/* the sky near the zenith, where a flat panel (a bonnet, a roof) looks, is not one lift: the clear sky there stays well under the sun's, and
 * the clouds stand out of it by a good part of the range, so a panel's reflection is patchy and not uniformly bright (a saturated paint read the
 * uniform lift as a flat change of colour) */
static void test_the_high_sky_has_clouds_standing_out_of_a_modest_clear_sky(void)
{
    const uint8_t *image = car_envmap_image();
    int lo = 255, hi = 0;
    for (int i = 0; i < 96; i++) {
        float r[3], uv[2];
        reflection_at(0.8f, (float)i * 0.0654f, r);
        uv_of_reflection(r, uv);
        const int b = car_envmap_sample(image, uv[0], uv[1]);
        lo = b < lo ? b : lo, hi = b > hi ? b : hi;
    }
    CHECK(lo <= 128 + 70);        /* the clear sky: at most 70 bytes above the neutral */
    CHECK(hi - lo >= 40);         /* a cloud rises 40 bytes or more out of it */
}

/* the dark band just under the horizon is darker than the flat ground further down */
static void test_the_band_under_the_horizon_is_darker_than_the_ground(void)
{
    const uint8_t *image = car_envmap_image();
    float band[3], ground[3], uv[2];
    reflection_at(-0.05f, 0.9f, band);
    reflection_at(-0.7f, 0.9f, ground);
    uv_of_reflection(band, uv);
    const uint8_t b = car_envmap_sample(image, uv[0], uv[1]);
    uv_of_reflection(ground, uv);
    const uint8_t g = car_envmap_sample(image, uv[0], uv[1]);
    CHECK(b < g);
    CHECK(g < 128);
}

/* the sun spot is the brightest texel: the texel the sun's direction maps to is the image's maximum */
static void test_the_sun_texel_is_the_brightest(void)
{
    const uint8_t *image = car_envmap_image();
    float uv[2];
    uv_of_reflection(SUN_CAMERA, uv);
    const uint8_t sun = car_envmap_sample(image, uv[0], uv[1]);
    uint8_t top = 0;
    for (int i = 0; i < S * S; i++) top = image[i] > top ? image[i] : top;
    CHECK(sun == top);
    CHECK(sun >= 240);
    float off[3] = { 0, 0.2f, -0.98f };   /* a sky direction well away from the sun */
    float far_uv[2];
    uv_of_reflection(off, far_uv);
    CHECK(sun > car_envmap_sample(image, far_uv[0], far_uv[1]));
}

/* the image is signed: some texels lift above the neutral and some shade below it (not additive only) */
static void test_the_image_is_signed(void)
{
    const uint8_t *image = car_envmap_image();
    int above = 0, below = 0;
    for (int i = 0; i < S * S; i++) {
        above += image[i] > 128;
        below += image[i] < 128;
    }
    CHECK(above > S * S / 4);
    CHECK(below > S * S / 4);
}

/* the neutral byte asks for the neutral level, a lift goes up to the lightest, a shade goes down, but never past the shade limit, and the
 * levels follow the bytes in order */
static void test_the_level_follows_the_byte(void)
{
    const float neutral = CAR_SHINE_NEUTRAL;
    const float limit = CAR_SHINE_NEUTRAL * (1.0f - CAR_ENVMAP_SHADE);
    CHECK(fabsf(car_envmap_level(128, CAR_ENVMAP_BODY_GAIN) - neutral) < 0.0001f);   /* the paint as it is */
    CHECK(car_envmap_level(0, CAR_ENVMAP_BODY_GAIN) < neutral);                       /* a dark byte shades */
    CHECK(car_envmap_level(0, CAR_ENVMAP_BODY_GAIN) >= limit - 0.0001f);             /* and not past the shade limit */
    CHECK(car_envmap_level(0, CAR_ENVMAP_GLASS_GAIN) >= limit - 0.0001f);
    CHECK(car_envmap_level(255, CAR_ENVMAP_GLASS_GAIN) > CAR_SHINE_LEVELS - 3.5f);
    CHECK(car_envmap_level(255, CAR_ENVMAP_BODY_GAIN) < car_envmap_level(255, CAR_ENVMAP_GLASS_GAIN));   /* the paint shines less than the glass */
    float last = -1.0f;
    for (int b = 0; b < 256; b++) {
        const float l = car_envmap_level((uint8_t)b, CAR_ENVMAP_BODY_GAIN);
        CHECK(l >= last);
        last = l;
    }
}

/* the steps between a table's neighbouring levels that jump far in colour are marked hard (they are not dithered): a red to a pink, not a red to
 * the next red; the lightest level has no step above it */
static void test_far_steps_are_hard(void)
{
    uint8_t palette[768] = { 0 };
    static uint8_t tab[CAR_SHINE_LEVELS][256], hard[CAR_SHINE_LEVELS][256];
    const uint8_t cols[5][3] = { { 0, 0, 0 }, { 251, 56, 52 }, { 186, 89, 81 }, { 178, 24, 24 }, { 158, 4, 8 } };
    for (int e = 0; e < 5; e++)
        for (int k = 0; k < 3; k++) palette[3 * e + k] = cols[e][k];
    for (int l = 0; l < CAR_SHINE_LEVELS; l++)
        for (int i = 0; i < 256; i++) tab[l][i] = (uint8_t)i;
    tab[5][1] = 1, tab[6][1] = 2;    /* red to pink: 79 apart */
    tab[5][3] = 3, tab[6][3] = 4;    /* red to a darker red: 32 apart */
    tab[CAR_SHINE_LEVELS - 1][1] = 2;
    car_envmap_hard(palette, tab, hard);
    CHECK_EQ_INT(hard[5][1], 1);
    CHECK_EQ_INT(hard[5][3], 0);
    CHECK_EQ_INT(hard[4][1], 0);                          /* the same entry at both levels */
    CHECK_EQ_INT(hard[CAR_SHINE_LEVELS - 1][1], 0);       /* no level above the lightest */
}

/* the PNG override (TNFS_CAR_ENVMAP, a full path): a 2 x 2 grey picture (its top left texel light, the rest dark) fills the image by nearest
 * neighbour, its top row the image's top */
static void write_png(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h)
{
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(png_write_rgba(rgba, w, h, &png, &len) == PNG_OK);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(png, 1, len, f) == len);
        fclose(f);
    }
    png_free(png);
}

static const char *const OVERRIDE = "envmap_override_test.png";
static const char *const NOT_PNG = "envmap_not_png_test.txt";

static void test_the_png_override_is_used_when_present(void)
{
    const uint8_t rgba[2 * 2 * 4] = { 200, 200, 200, 255, 40, 40, 40, 255, 40, 40, 40, 255, 40, 40, 40, 255 };
    write_png(OVERRIDE, rgba, 2, 2);
    setenv("TNFS_CAR_ENVMAP", OVERRIDE, 1);
    static uint8_t image[S * S];
    car_envmap_build(image);
    unsetenv("TNFS_CAR_ENVMAP");
    CHECK_EQ_INT(image[0], 200);                          /* top left quarter: the light texel */
    CHECK_EQ_INT(image[(S / 2 - 1) * S + S / 2 - 1], 200);
    CHECK_EQ_INT(image[S * S - 1], 40);                   /* bottom right: the dark one */
    CHECK_EQ_INT(image[(S - 1) * S], 40);
    remove(OVERRIDE);
}

/* a missing or unreadable override falls back to the procedural image, byte for byte */
static void test_the_override_falls_back_when_missing_or_invalid(void)
{
    const uint8_t *procedural = car_envmap_image();   /* made with no override set */
    static uint8_t image[S * S];
    setenv("TNFS_CAR_ENVMAP", "envmap_no_such_file_test.png", 1);
    car_envmap_build(image);
    CHECK(memcmp(image, procedural, sizeof image) == 0);
    FILE *f = fopen(NOT_PNG, "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        fputs("this is not a png", f);
        fclose(f);
    }
    setenv("TNFS_CAR_ENVMAP", NOT_PNG, 1);
    car_envmap_build(image);
    unsetenv("TNFS_CAR_ENVMAP");
    CHECK(memcmp(image, procedural, sizeof image) == 0);
    remove(NOT_PNG);
}

int main(void)
{
    unsetenv("TNFS_CAR_ENVMAP");
    test_far_steps_are_hard();
    test_facing_the_viewer_is_the_centre();
    test_turned_to_the_right_maps_right_of_centre();
    test_an_up_facing_panel_reflects_the_sky();
    test_a_reflection_below_the_horizon_maps_below_the_centre();
    test_a_camera_roll_does_not_move_the_sky();
    test_pack_and_unpack_round_trip();
    test_the_sky_is_above_neutral_and_the_ground_below();
    test_the_sky_is_brighter_toward_the_zenith();
    test_the_high_sky_has_clouds_standing_out_of_a_modest_clear_sky();
    test_the_band_under_the_horizon_is_darker_than_the_ground();
    test_the_sun_texel_is_the_brightest();
    test_the_image_is_signed();
    test_the_level_follows_the_byte();
    test_the_png_override_is_used_when_present();
    test_the_override_falls_back_when_missing_or_invalid();
    return TEST_REPORT();
}
