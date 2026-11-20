/* The car's night view (formats/car_night.h) on a synthetic texture: the tint, the lamp states, the fixed lamp colours, the police flash. */
#include "formats/car_mask_codec.h"
#include "formats/car_night.h"
#include "formats/lamp_emissive.h"
#include "formats/police_flash.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static uint8_t palette[768];

static void rgb_of(const uint8_t *rgba, int i, uint8_t out[3])
{
    out[0] = rgba[4 * i], out[1] = rgba[4 * i + 1], out[2] = rgba[4 * i + 2];
}

static int same(const uint8_t a[3], int r, int g, int b)
{
    return a[0] == r && a[1] == g && a[2] == b;
}

static int up(int v) { return v << 2 | v >> 4; }

/* texels: 0 body (index 40), 1 headlight, 2 position, 3 brake, 4 reverse, 5 a police light entry, 6 transparent */
static const uint8_t INDEX[8] = { 40, 50, 51, 52, 53, 0xfa, 0xff, 40 };
static const uint8_t MASK[8] = { CAR_MASK_PAINT, CAR_MASK_HEADLIGHT, CAR_MASK_POSITION, CAR_MASK_BRAKE, CAR_MASK_REVERSE, CAR_MASK_PAINT, CAR_MASK_PAINT, CAR_MASK_MATTE };

static void test_day_is_the_palette(void)
{
    CarNightState s;
    memset(&s, 0, sizeof s);
    uint8_t rgba[32], six[3];
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    uint8_t px[3];
    rgb_of(rgba, 0, px);
    for (int k = 0; k < 3; k++) six[k] = (uint8_t)(palette[3 * 40 + k] >> 2);
    CHECK(same(px, up(six[0]), up(six[1]), up(six[2])));   /* 6 bits and back, as the screen has it */
    CHECK(rgba[3] == 255 && rgba[4 * 6 + 3] == 0);          /* entry 0xff is transparent */
    rgb_of(rgba, 1, px);
    CHECK(!same(px, up(LAMP_HEADLIGHT_R), up(LAMP_HEADLIGHT_G), up(LAMP_HEADLIGHT_B)));   /* by day the head light is a plain texel */
}

static void test_lamps_at_night(void)
{
    CarNightState s;
    memset(&s, 0, sizeof s);
    s.night = 1, s.lights = 1;
    uint8_t rgba[32], px[3];
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    rgb_of(rgba, 1, px);
    CHECK(same(px, up(LAMP_HEADLIGHT_R), up(LAMP_HEADLIGHT_G), up(LAMP_HEADLIGHT_B)));   /* the head light: warm white (63,58,44 in 6 bits) */
    rgb_of(rgba, 2, px);
    CHECK(same(px, up(LAMP_POSITION_R), up(LAMP_POSITION_G), up(LAMP_POSITION_B)));
    rgb_of(rgba, 3, px);
    CHECK(same(px, up(LAMP_POSITION_R), up(LAMP_POSITION_G), up(LAMP_POSITION_B)));       /* an unlit brake shows the position light */
    s.brake = 1;
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    rgb_of(rgba, 3, px);
    CHECK(same(px, up(LAMP_BRAKE_R), up(LAMP_BRAKE_G), up(LAMP_BRAKE_B)));                 /* braking: the lit brake */
    rgb_of(rgba, 4, px);
    CHECK(!same(px, up(LAMP_REVERSE_R), up(LAMP_REVERSE_G), up(LAMP_REVERSE_B)));          /* the reversing light is off */
    s.reverse = 1;
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    rgb_of(rgba, 4, px);
    CHECK(same(px, up(LAMP_REVERSE_R), up(LAMP_REVERSE_G), up(LAMP_REVERSE_B)));
    CHECK(car_night_lamp_state(&s) == (LAMP_BRAKING | LAMP_REVERSING));
}

static void test_reverse_by_day(void)
{
    CarNightState s;
    memset(&s, 0, sizeof s);
    s.reverse = 1, s.lights = 1;   /* by day the head and position lights do not glow, the reversing light does */
    uint8_t rgba[32], px[3];
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    rgb_of(rgba, 4, px);
    CHECK(same(px, up(LAMP_REVERSE_R), up(LAMP_REVERSE_G), up(LAMP_REVERSE_B)));
    rgb_of(rgba, 1, px);
    CHECK(!same(px, up(LAMP_HEADLIGHT_R), up(LAMP_HEADLIGHT_G), up(LAMP_HEADLIGHT_B)));
    CHECK(car_night_lamp_state(&s) == (LAMP_DAY | LAMP_REVERSING));
}

/* L1 (the owner's test): by day the brake lights light as in the race (car_lights.c writes the car's lamp entry with the lit row in any light), the head and position
 * lights do not */
static void test_brake_by_day(void)
{
    CarNightState s;
    memset(&s, 0, sizeof s);
    s.brake = 1, s.lights = 1;
    uint8_t rgba[32], px[3];
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    rgb_of(rgba, 3, px);
    CHECK(same(px, up(LAMP_BRAKE_R), up(LAMP_BRAKE_G), up(LAMP_BRAKE_B)));
    rgb_of(rgba, 1, px);
    CHECK(!same(px, up(LAMP_HEADLIGHT_R), up(LAMP_HEADLIGHT_G), up(LAMP_HEADLIGHT_B)));
    rgb_of(rgba, 2, px);
    CHECK(!same(px, up(LAMP_POSITION_R), up(LAMP_POSITION_G), up(LAMP_POSITION_B)));
    s.brake = 0;   /* off by day: the plain texel */
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    rgb_of(rgba, 3, px);
    CHECK(!same(px, up(LAMP_BRAKE_R), up(LAMP_BRAKE_G), up(LAMP_BRAKE_B)) && !same(px, up(LAMP_POSITION_R), up(LAMP_POSITION_G), up(LAMP_POSITION_B)));
}

static void test_light_at_car(void)
{
    CarNightState s;
    memset(&s, 0, sizeof s);
    uint8_t rgba[32], lit[32], px[3], q[3];
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    s.lit = 1, s.light[0] = s.light[1] = s.light[2] = 0.5f;
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, lit));
    rgb_of(rgba, 0, q), rgb_of(lit, 0, px);
    CHECK(px[0] < q[0] && px[1] < q[1]);                  /* a body texel is darker under half the light */
    for (int k = 0; k < 3; k++) CHECK(abs((int)px[k] - (int)q[k] / 2) <= 4);
    s.night = 1, s.lights = 1;
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, lit));
    rgb_of(lit, 1, px);
    CHECK(same(px, up(LAMP_HEADLIGHT_R), up(LAMP_HEADLIGHT_G), up(LAMP_HEADLIGHT_B)));   /* a lamp is lit whatever the light (the race's emissive entries) */
    s.light[0] = s.light[1] = s.light[2] = 3.0f;
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, lit));
    rgb_of(lit, 0, px);
    for (int k = 0; k < 3; k++) CHECK(px[k] >= q[k]);   /* more light than the file's is brighter, clamped at 6 bits (255) */
}

static void test_tint_and_police(void)
{
    CarNightState s;
    memset(&s, 0, sizeof s);
    s.tinted = 1, s.tint = (PaletteTint){ 0x6000, 0x50, 0x4000, 0x60, 0xa0 };
    uint8_t out[768], rgba[32], px[3];
    car_night_palette(palette, &s, out);
    uint8_t plain[768];
    memset(&s.tint, 0, 0), s.tinted = 0;
    car_night_palette(palette, &s, plain);
    CHECK(memcmp(out, plain, 768) != 0);   /* the tint changes the colours */
    s.tinted = 1;
    s.police = 1, s.police_tick = 0;
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    uint8_t flash[256];
    police_lights_table(NULL, palette, 0, flash);
    rgb_of(rgba, 5, px);
    CHECK(same(px, palette[3 * flash[0xfa]], palette[3 * flash[0xfa] + 1], palette[3 * flash[0xfa] + 2]));   /* the flash's entry, untinted */
    s.police_tick = 15;
    CHECK(car_night_texture(INDEX, MASK, 8, 1, palette, &s, rgba));
    rgb_of(rgba, 5, px);
    police_lights_table(NULL, palette, 15, flash);
    CHECK(same(px, palette[3 * flash[0xfa]], palette[3 * flash[0xfa] + 1], palette[3 * flash[0xfa] + 2]));   /* and the other colour in the other phase */
    CHECK(!car_night_texture(NULL, MASK, 8, 1, palette, &s, rgba) && !car_night_texture(INDEX, MASK, 0, 1, palette, &s, rgba));
}

int main(void)
{
    for (int i = 0; i < 256; i++) palette[3 * i] = (uint8_t)(i * 7), palette[3 * i + 1] = (uint8_t)(255 - i), palette[3 * i + 2] = (uint8_t)(i * 3);
    palette[3 * 0xfa] = 252, palette[3 * 0xfa + 1] = 100, palette[3 * 0xfa + 2] = 84;
    test_day_is_the_palette();
    test_lamps_at_night();
    test_reverse_by_day();
    test_brake_by_day();
    test_light_at_car();
    test_tint_and_police();
    return TEST_REPORT();
}
