/* The PS1 second livery's rules on synthetic colours (no CD data): the colour map by luminance rank, the palette remap
 * for both palette types, the choice a rival makes, and the extras.cfg and port.cfg keys that switch it on. */
#include "game/race/car_livery.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* the port.cfg and extras.cfg the parsers under test are read from; the tests parse text and never load a file, so the
 * platform's user-file calls are stubs (as test_dash_box.c has them) */
uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    (void)name, (void)size;
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

enum { ORIGIN_A = 0x0010, BODY_B = 0x0200, SHARED = 0x4210, BODY_C = 0x0100, BODY_D = 0x0008 };

static void test_map(void)
{
    uint32_t *normal = calloc(PSX_COLOURS, sizeof *normal), *alt = calloc(PSX_COLOURS, sizeof *alt);
    CarLiveryPair pairs[CAR_LIVERY_PAIRS];
    CHECK(normal != NULL && alt != NULL);
    /* normal: A (luma 3840, weight 10) and B (luma 7552, weight 4) are body; SHARED is on both liveries */
    normal[ORIGIN_A] = 10, normal[BODY_B] = 4, normal[SHARED] = 50;
    /* alternate: D (luma 1920, weight 3) and C (luma 3776, weight 7) are body; SHARED is on both */
    alt[BODY_C] = 7, alt[BODY_D] = 3, alt[SHARED] = 50;
    const int count = car_livery_map(normal, alt, NULL, 0, pairs);
    CHECK_EQ_INT(count, 2);
    /* A's midpoint is 5 of 14 pixels: the alternate's D covers the first 3, so C; B's is 12 of 14: C again */
    CHECK_EQ_INT(pairs[0].from, ORIGIN_A);
    CHECK_EQ_INT(pairs[0].to, BODY_C);
    CHECK_EQ_INT(pairs[1].from, BODY_B);
    CHECK_EQ_INT(pairs[1].to, BODY_C);
    /* a livery with no colour of its own has nothing to map */
    memset(alt, 0, PSX_COLOURS * sizeof *alt);
    alt[SHARED] = 50;
    CHECK_EQ_INT(car_livery_map(normal, alt, NULL, 0, pairs), 0);
    /* a forced colour the alternate also has (SHARED, not body by count) joins the body, weighing the mean body
     * colour: with one normal body colour A (weight 10) and the alternate's C, the forced SHARED maps to C as well */
    memset(alt, 0, PSX_COLOURS * sizeof *alt);
    memset(normal, 0, PSX_COLOURS * sizeof *normal);
    alt[BODY_C] = 7;
    alt[SHARED] = 50;
    normal[ORIGIN_A] = 10, normal[SHARED] = 50;
    const uint16_t forced[1] = { SHARED };
    CHECK_EQ_INT(car_livery_map(normal, alt, forced, 1, pairs), 2);
    CHECK_EQ_INT(pairs[0].from, ORIGIN_A);
    CHECK_EQ_INT(pairs[0].to, BODY_C);
    CHECK_EQ_INT(pairs[1].from, SHARED);
    CHECK_EQ_INT(pairs[1].to, BODY_C);
    free(normal);
    free(alt);
}

static void test_remap(void)
{
    CarLiveryPair pairs[1] = { { ORIGIN_A, BODY_C } };
    uint8_t pal[256 * 3];
    memset(pal, 0, sizeof pal);
    /* an 8-bit palette: entry 0 is A (5-bit channels 16, 0, 0 in 8 bits: 128, 0, 0); entry 1 black stays */
    pal[0] = 128;
    car_livery_remap(pal, 0x24, pairs, 1);
    CHECK_EQ_INT(pal[0], 0);
    CHECK_EQ_INT(pal[1], 64);    /* C is green: 8 in its red field is 0, 8 in green is 64 */
    CHECK_EQ_INT(pal[2], 0);
    CHECK_EQ_INT(pal[3], 0);
    /* a 6-bit palette: the same colour is 32 (8-bit 128 >> 2), and comes back 6-bit: green 64 >> 2 = 16 */
    memset(pal, 0, sizeof pal);
    pal[0] = 32;
    car_livery_remap(pal, 0x22, pairs, 1);
    CHECK_EQ_INT(pal[0], 0);
    CHECK_EQ_INT(pal[1], 16);
    /* another palette type is left alone */
    memset(pal, 0, sizeof pal);
    pal[0] = 128;
    car_livery_remap(pal, 0x31, pairs, 1);
    CHECK_EQ_INT(pal[0], 128);
}

static void test_pick(void)
{
    /* a rival in the player's car always wears the other livery */
    CHECK_EQ_INT(car_livery_pick(0x1234, 3, 5, 5, 0), 1);
    CHECK_EQ_INT(car_livery_pick(0x1234, 3, 5, 5, 1), 0);
    /* any other rival: the same answer every time for the same seed and slot, and both answers across seeds */
    int seen[2] = { 0, 0 };
    for (uint32_t seed = 0; seed < 64; seed++) {
        const int first = car_livery_pick(seed, 2, 4, 5, 0);
        CHECK_EQ_INT(car_livery_pick(seed, 2, 4, 5, 0), first);
        seen[first] = 1;
    }
    CHECK(seen[0] && seen[1]);
    /* the --drive seed (0x2b67) over the slots: both livery answers come up, not one for every slot */
    int drive[2] = { 0, 0 };
    for (int slot = 1; slot <= 8; slot++) drive[car_livery_pick(0x2b67, slot, slot, -1, 0)] = 1;
    CHECK(drive[0] && drive[1]);
}

static void test_cfg(void)
{
    PortExtras d = port_extras_defaults();
    CHECK_EQ_INT(d.psx_liveries, 0);
    const char *on = "psx_liveries=1\n";
    CHECK_EQ_INT(port_extras_parse(on, strlen(on)).psx_liveries, 1);
    const char *junk = "psx_liveries=2\n";
    CHECK_EQ_INT(port_extras_parse(junk, strlen(junk)).psx_liveries, 0);
    char text[1024];
    CHECK(port_extras_format(&d, text, sizeof text) > 0);
    CHECK(strstr(text, "psx_liveries=0\n") != NULL);

    PortSettings s = port_settings_defaults();
    CHECK_EQ_INT((int)strlen(s.psx_dir), 0);
    const char *dir = "psx_dir=/Volumes/PS1 CD/car files\nfov_adjust=2\n";
    PortSettings p = port_settings_parse(dir, strlen(dir));
    CHECK(strcmp(p.psx_dir, "/Volumes/PS1 CD/car files") == 0);
    CHECK_EQ_INT(p.fov_adjust, 2);
    char out[512];
    CHECK(port_settings_format(&p, out, sizeof out) > 0);
    CHECK(strstr(out, "psx_dir=/Volumes/PS1 CD/car files\n") != NULL);
}

/* the car select's Color line: the per-car mask in extras.cfg, the swatch's colour, and the key's press edge */
static void test_select(void)
{
    CHECK_EQ_INT(psx_livery_car_on(0, 3), 0);
    CHECK_EQ_INT(psx_livery_car_on(psx_livery_mask_set(0, 3, 1), 3), 1);
    CHECK_EQ_INT(psx_livery_car_on(psx_livery_mask_set(0, 3, 1), 4), 0);
    CHECK_EQ_INT(psx_livery_mask_set(psx_livery_mask_set(0, 3, 1), 3, 0), 0);
    CHECK_EQ_INT(psx_livery_car_on(psx_livery_mask_set(0, 11, 1), 11), 1);
    CHECK_EQ_INT(psx_livery_mask_set(0, 12, 1), 0);
    CHECK_EQ_INT(psx_livery_car_on(0, -1), 0);

    PortExtras d = port_extras_defaults();
    CHECK_EQ_INT(d.psx_livery_cars, 0);
    const char *mask = "psx_livery_cars=2056\n";
    CHECK_EQ_INT(port_extras_parse(mask, strlen(mask)).psx_livery_cars, 2056);
    const char *big = "psx_livery_cars=4096\n";
    CHECK_EQ_INT(port_extras_parse(big, strlen(big)).psx_livery_cars, 0);
    const char *neg = "psx_livery_cars=-1\n";
    CHECK_EQ_INT(port_extras_parse(neg, strlen(neg)).psx_livery_cars, 0);
    d.psx_livery_cars = 2056;
    char text[512];
    CHECK(port_extras_format(&d, text, sizeof text) > 0);
    CHECK(strstr(text, "psx_livery_cars=2056\n") != NULL);
    CHECK_EQ_INT(port_extras_parse(text, strlen(text)).psx_livery_cars, 2056);

    /* the swatch: the mapped colour the alternate uses most; none when it uses none of them */
    static uint32_t alternate[PSX_COLOURS];
    CarLiveryPair pairs[2] = { { ORIGIN_A, BODY_B }, { BODY_C, BODY_D } };
    uint16_t colour = 0;
    CHECK_EQ_INT(car_livery_main(pairs, 2, alternate, &colour), 0);
    alternate[BODY_B] = 5, alternate[BODY_D] = 9;
    CHECK_EQ_INT(car_livery_main(pairs, 2, alternate, &colour), 1);
    CHECK_EQ_INT(colour, BODY_D);
    alternate[BODY_B] = 9;
    CHECK_EQ_INT(car_livery_main(pairs, 2, alternate, &colour), 1);
    CHECK_EQ_INT(colour, BODY_B);

    /* the key: one press is one edge, a held key is not again */
    int was = 0;
    CHECK_EQ_INT(car_livery_pressed(0, &was), 0);
    CHECK_EQ_INT(car_livery_pressed(1, &was), 1);
    CHECK_EQ_INT(car_livery_pressed(1, &was), 0);
    CHECK_EQ_INT(car_livery_pressed(0, &was), 0);
    CHECK_EQ_INT(car_livery_pressed(1, &was), 1);
}

int main(void)
{
    test_map();
    test_remap();
    test_pick();
    test_cfg();
    test_select();
    return TEST_REPORT();
}
