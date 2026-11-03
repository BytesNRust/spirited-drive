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
    char out[1024];
    CHECK(port_settings_format(&p, out, sizeof out) > 0);
    CHECK(strstr(out, "psx_dir=/Volumes/PS1 CD/car files\n") != NULL);
}

/* the per-car mask in extras.cfg and the swatch's colour */
static void test_select(void)
{
    PortExtras d = port_extras_defaults();
    for (int c = 0; c < CAR_PAINT_SLOTS; c++) CHECK_EQ_INT(d.car_paint[c], PAINT_OWN);
    const char *keyed = "car_paint=0a3\n";
    PortExtras p = port_extras_parse(keyed, strlen(keyed));
    CHECK_EQ_INT(p.car_paint[0], 0);
    CHECK_EQ_INT(p.car_paint[1], 10);
    CHECK_EQ_INT(p.car_paint[2], 3);
    CHECK_EQ_INT(p.car_paint[3], 0);
    const char *bad = "car_paint=1z2\n";   /* a character that is no digit ends the read */
    p = port_extras_parse(bad, strlen(bad));
    CHECK_EQ_INT(p.car_paint[0], 1);
    CHECK_EQ_INT(p.car_paint[2], 0);
    const char *old = "psx_livery_cars=2056\n";   /* bits 3 and 11 */
    p = port_extras_parse(old, strlen(old));
    CHECK_EQ_INT(p.car_paint[3], PAINT_PS1);
    CHECK_EQ_INT(p.car_paint[11], PAINT_PS1);
    CHECK_EQ_INT(p.car_paint[4], PAINT_OWN);
    const char *both = "psx_livery_cars=2056\ncar_paint=000000000000\n";   /* the new key rules: the choices were changed back */
    CHECK_EQ_INT(port_extras_parse(both, strlen(both)).car_paint[3], PAINT_OWN);
    const char *big = "psx_livery_cars=4096\n";
    CHECK_EQ_INT(port_extras_parse(big, strlen(big)).car_paint[0], PAINT_OWN);
    d.car_paint[2] = 4, d.car_paint[11] = PAINT_PS1, d.psx_livery_cars = 2056;
    char text[512];
    CHECK(port_extras_format(&d, text, sizeof text) > 0);
    CHECK(strstr(text, "car_paint=004000000001\n") != NULL);
    CHECK(strstr(text, "psx_livery_cars") == NULL);   /* the old key is no longer written */
    CHECK_EQ_INT(port_extras_parse(text, strlen(text)).car_paint[2], 4);
    CHECK_EQ_INT(port_extras_parse(text, strlen(text)).car_paint[11], PAINT_PS1);

    /* the swatch: the mapped colour the alternate uses most; none when it uses none of them */
    static uint32_t alternate[PSX_COLOURS];
    CarLiveryPair pairs[2] = { { ORIGIN_A, BODY_B }, { BODY_C, BODY_D } };
    uint16_t colour = 0;
    CHECK_EQ_INT(car_livery_main(pairs, 2, alternate, &colour), 0);
    alternate[BODY_B] = 5, alternate[BODY_D] = 90;   /* B is under the weight floor: the heaviest, D */
    CHECK_EQ_INT(car_livery_main(pairs, 2, alternate, &colour), 1);
    CHECK_EQ_INT(colour, BODY_D);
    alternate[BODY_B] = 90;
    CHECK_EQ_INT(car_livery_main(pairs, 2, alternate, &colour), 1);
    CHECK_EQ_INT(colour, BODY_B);

    /* the normal livery's main colour: the mapped colour the normal livery uses most */
    static uint32_t normal[PSX_COLOURS];
    CHECK_EQ_INT(car_livery_main_from(pairs, 2, normal, &colour), 0);
    normal[ORIGIN_A] = 3, normal[BODY_C] = 80;
    CHECK_EQ_INT(car_livery_main_from(pairs, 2, normal, &colour), 1);
    CHECK_EQ_INT(colour, BODY_C);

    /* the paint, not the shading: a heavy dark shade beside a lighter saturated tone shows the saturated one (the real
     * Ferrari's normal was (24,16,16) with the red under it); a tone too rare to be the body does not count */
    enum { SHADE = 0x0843, RED = 0x001f /* (248,0,0) */, GREY_B = 0x6318 /* (192,192,192) */, TINY = 0x03e0 };
    static uint32_t shaded[PSX_COLOURS];
    const CarLiveryPair paint[3] = { { SHADE, SHADE }, { RED, RED }, { GREY_B, GREY_B } };
    shaded[SHADE] = 1000, shaded[RED] = 100;
    CHECK_EQ_INT(car_livery_main(paint, 3, shaded, &colour), 1);
    CHECK_EQ_INT(colour, RED);
    CHECK_EQ_INT(car_livery_main_from(paint, 3, shaded, &colour), 1);
    CHECK_EQ_INT(colour, RED);
    shaded[RED] = 10;                                   /* under a sixteenth of the shade's weight: a stray */
    CHECK_EQ_INT(car_livery_main(paint, 3, shaded, &colour), 1);
    CHECK_EQ_INT(colour, SHADE);
    /* one hue in three tones: the paint's middle tone, not the shade and not the highlight (the real Ferrari showed a
     * dusty pink: the specular highlight). The weighted median by luminance of the hue's colours. */
    enum { MAROON = 0x000c /* (96,0,0) */, MID = 0x1497 /* (184,32,40) */, HIGHLIGHT = 0x35b8 /* (192,104,104) */,
           BLUE = 0x64a3 /* (24,40,200) */ };
    static uint32_t tones[PSX_COLOURS];
    const CarLiveryPair hues[4] = { { MAROON, MAROON }, { MID, MID }, { HIGHLIGHT, HIGHLIGHT }, { BLUE, BLUE } };
    tones[MAROON] = 400, tones[MID] = 300, tones[HIGHLIGHT] = 150;
    CHECK_EQ_INT(car_livery_main(hues, 4, tones, &colour), 1);
    CHECK_EQ_INT(colour, MID);
    tones[MAROON] = 1000, tones[MID] = 100, tones[HIGHLIGHT] = 50;   /* the shade carries the weight: the shade */
    CHECK_EQ_INT(car_livery_main(hues, 4, tones, &colour), 1);
    CHECK_EQ_INT(colour, MAROON);
    tones[MAROON] = 400, tones[MID] = 300, tones[HIGHLIGHT] = 150, tones[BLUE] = 380;   /* another hue is not averaged in */
    CHECK_EQ_INT(car_livery_main(hues, 4, tones, &colour), 1);
    CHECK_EQ_INT(colour, MID);
    tones[BLUE] = 0, tones[MID] = 20;                    /* under the floor: out */
    CHECK_EQ_INT(car_livery_main(hues, 4, tones, &colour), 1);
    CHECK_EQ_INT(colour, MAROON);
    shaded[RED] = 0, shaded[GREY_B] = 400;              /* only greys: the heaviest, as before */
    CHECK_EQ_INT(car_livery_main(paint, 3, shaded, &colour), 1);
    CHECK_EQ_INT(colour, SHADE);
}

/* the body set of a car from its textures: the entries four fifths or more in the body textures, not near black */
static void test_body_set(void)
{
    uint32_t total[256] = { 0 }, body[256] = { 0 };
    uint8_t palette[768] = { 0 }, out[256];
    for (int i = 0; i < 256; i++) palette[i * 3] = palette[i * 3 + 1] = palette[i * 3 + 2] = (uint8_t)(255 - i);   /* a grey ramp, 8 bits */
    total[1] = 100, body[1] = 100;      /* light, all body: in */
    total[2] = 100, body[2] = 80;       /* exactly four fifths: in */
    total[3] = 100, body[3] = 79;       /* under: out (a window's grey shared with the paint) */
    total[4] = 100, body[4] = 0;        /* a tyre's: out */
    total[60] = 100, body[60] = 100;    /* mid grey (195): in, whatever the lightness */
    total[250] = 100, body[250] = 100;  /* (5,5,5): nearly black, out */
    total[7] = 0;                       /* unused: out */
    const int n = car_livery_body_set(total, body, palette, 0x24, out);
    CHECK_EQ_INT(n, 3);
    CHECK_EQ_INT(out[0], 1);
    CHECK_EQ_INT(out[1], 2);
    CHECK_EQ_INT(out[2], 60);
    CHECK_EQ_INT(car_livery_body_set(total, total, palette, 0x31, out), 0);   /* a palette of another type: none */
}

int main(void)
{
    test_map();
    test_remap();
    test_pick();
    test_cfg();
    test_select();
    test_body_set();
    return TEST_REPORT();
}
