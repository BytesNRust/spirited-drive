/* The emissive lamp texels (lamp_emissive.h): collected from a synthetic mask, pointed at the low-light entries and back, and the free-entry scan. */
#include "tests/test.h"

#include "game/race/car_mask.h"
#include "game/race/lamp_emissive.h"

#include <string.h>

static void test_collect_and_toggle(void)
{
    const uint8_t mask[8] = { CAR_MASK_PAINT, CAR_MASK_BRAKE, CAR_MASK_MATTE, CAR_MASK_POSITION, CAR_MASK_HEADLIGHT, CAR_MASK_BLACK, CAR_MASK_SHINE, CAR_MASK_BRAKE };
    uint8_t file[20];
    for (int i = 0; i < 20; i++) file[i] = (uint8_t)(100 + i);
    LampTexels l = { 0 };
    CHECK_EQ_INT(lamp_texels_collect(&l, 10, mask, 8), 4);   /* the texels at file offsets 11, 13, 14 and 17 */
    CHECK_EQ_INT(l.texel[0].offset, 11);
    CHECK_EQ_INT(l.texel[1].kind, LAMP_POSITION);
    CHECK_EQ_INT(l.texel[2].kind, LAMP_HEADLIGHT);
    lamp_texels_read(&l, file, sizeof file);
    CHECK_EQ_INT(l.texel[3].day, 117);
    const LampEntries e = { 0xfe, 0xf0, 0xf1, 0 };   /* brake lit, position, headlight, reversing */
    uint8_t before[20];
    memcpy(before, file, sizeof file);
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_BRAKING, &e), 4);
    CHECK_EQ_INT(file[11], 0xfe);
    CHECK_EQ_INT(file[13], 0xf0);
    CHECK_EQ_INT(file[14], 0xf1);
    CHECK_EQ_INT(file[17], 0xfe);
    CHECK_EQ_INT(file[12], 112);   /* the other texels are left alone */
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_BRAKING, &e), 0);   /* idempotent */
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_LOW, &e), 2);      /* the brake released: its texels show the position entry */
    CHECK_EQ_INT(file[11], 0xf0);
    CHECK_EQ_INT(file[17], 0xf0);
    CHECK_EQ_INT(file[14], 0xf1);
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_DAY, &e), 4);   /* day: byte for byte what it was */
    CHECK_MEM(file, before, sizeof file);
    lamp_texels_free(&l);
}

/* the reversing light (class 252): lit with LAMP_REVERSING in any light, the day's too, and the day's index otherwise; the other kinds do not react to it */
static void test_reversing_light_lit_only_in_reverse(void)
{
    const uint8_t mask[4] = { CAR_MASK_REVERSE, CAR_MASK_BRAKE, CAR_MASK_PAINT, CAR_MASK_REVERSE };
    uint8_t file[6];
    for (int i = 0; i < 6; i++) file[i] = (uint8_t)(50 + i);
    LampTexels l = { 0 };
    CHECK_EQ_INT(lamp_texels_collect(&l, 1, mask, 4), 3);   /* offsets 1, 2 and 4 */
    CHECK_EQ_INT(l.texel[0].kind, LAMP_REVERSE);
    CHECK_EQ_INT(l.texel[1].kind, LAMP_BRAKE);
    CHECK_EQ_INT(l.texel[2].kind, LAMP_REVERSE);
    lamp_texels_read(&l, file, sizeof file);
    const LampEntries e = { 0xfe, 0xf0, 0xf1, 0xf2 };   /* brake lit, position, headlight, reversing */
    uint8_t before[6];
    memcpy(before, file, sizeof file);
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_DAY, &e), 0);   /* day, not reversing: nothing */
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_LOW, &e), 1);   /* low light: the brake's texel only */
    CHECK_EQ_INT(file[1], 51);                                                 /* the reversing light is not lit by the dark */
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_DAY, &e), 1);
    CHECK_MEM(file, before, sizeof file);
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_DAY | LAMP_REVERSING, &e), 2);   /* day, in reverse: lit */
    CHECK_EQ_INT(file[1], 0xf2);
    CHECK_EQ_INT(file[4], 0xf2);
    CHECK_EQ_INT(file[2], 52);                                                 /* the brake texel stays the day's */
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_BRAKING | LAMP_REVERSING, &e), 1);   /* low light, braking, in reverse: both */
    CHECK_EQ_INT(file[2], 0xfe);
    CHECK_EQ_INT(file[1], 0xf2);
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_DAY, &e), 3);   /* out of reverse: the day's again */
    CHECK_MEM(file, before, sizeof file);
    const LampEntries none = { 0, 0, 0, 0 };                                    /* no free entry: the texel keeps the day's even in reverse */
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_DAY | LAMP_REVERSING, &none), 0);
    lamp_texels_free(&l);
}

/* four shared entries can be picked */
static void test_four_entries(void)
{
    uint8_t used[256] = { 0 }, usable[256], rgb[768] = { 0 }, out[4] = { 0 };
    memset(usable, 1, sizeof usable);
    for (int e = 0; e < 256; e++) rgb[3 * e] = rgb[3 * e + 1] = rgb[3 * e + 2] = (uint8_t)e;
    CHECK_EQ_INT(lamp_pick_entries(used, usable, rgb, 0x30, 0xef, 4, out), 4);
    for (int i = 0; i < 4; i++)
        for (int k = i + 1; k < 4; k++) CHECK(out[i] != out[k]);
}

static void test_offsets_past_the_file_are_dropped(void)
{
    const uint8_t mask[4] = { CAR_MASK_BRAKE, CAR_MASK_BRAKE, CAR_MASK_BRAKE, CAR_MASK_BRAKE };
    uint8_t file[2] = { 5, 6 };
    LampTexels l = { 0 };
    CHECK_EQ_INT(lamp_texels_collect(&l, 0, mask, 4), 4);
    lamp_texels_read(&l, file, sizeof file);
    CHECK_EQ_INT(l.count, 2);
    const LampEntries e = { 9, 9, 9, 9 };
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_BRAKING, &e), 2);
    CHECK_EQ_INT(file[1], 9);
    lamp_texels_free(&l);
}

static void test_no_lamps_no_change(void)
{
    const uint8_t mask[3] = { CAR_MASK_PAINT, CAR_MASK_MATTE, CAR_MASK_BLACK };
    LampTexels l = { 0 };
    CHECK_EQ_INT(lamp_texels_collect(&l, 0, mask, 3), 0);
    CHECK_EQ_INT(l.count, 0);
    lamp_texels_free(&l);
}

static void test_pick_entries(void)
{
    uint8_t used[256], usable[256] = { 0 }, rgb[768] = { 0 }, out[2];
    memset(used, 1, sizeof used);
    usable[0x10] = usable[0x20] = 1;
    rgb[3 * 0x20] = 100;                       /* the usable colours: 0 and 100 (red) */
    used[0x40] = used[0x50] = used[0xee] = 0;  /* free, and 3, 40 and 1 away from them */
    rgb[3 * 0x40] = 3, rgb[3 * 0x50] = 60, rgb[3 * 0xee] = 101;
    CHECK_EQ_INT(lamp_pick_entries(used, usable, rgb, 0x30, 0xef, 2, out), 2);
    CHECK(out[0] == 0xee && out[1] == 0x40);   /* the nearest first */
    CHECK_EQ_INT(lamp_pick_entries(used, usable, rgb, 0x30, 0xef, 1, out), 1);
    CHECK_EQ_INT(out[0], 0xee);
    uint8_t three[3];
    CHECK_EQ_INT(lamp_pick_entries(used, usable, rgb, 0x30, 0xef, 3, three), 3);
    CHECK(three[0] == 0xee && three[1] == 0x40 && three[2] == 0x50);
    CHECK_EQ_INT(lamp_pick_entries(used, usable, rgb, 0x30, 0xed, 2, out), 2);   /* inside the range only */
    CHECK(out[0] == 0x40 && out[1] == 0x50);
    used[0x60] = used[0x61] = 0;
    rgb[3 * 0x60] = rgb[3 * 0x61] = 5;
    used[0x40] = used[0x50] = used[0xee] = 1;
    CHECK_EQ_INT(lamp_pick_entries(used, usable, rgb, 0x30, 0xef, 1, out), 1);
    CHECK_EQ_INT(out[0], 0x61);                /* a tie goes to the higher entry */
    memset(used, 1, sizeof used);
    CHECK_EQ_INT(lamp_pick_entries(used, usable, rgb, 0, 255, 2, out), 0);
}

static void test_entry_zero_is_none(void)
{
    const uint8_t mask[3] = { CAR_MASK_BRAKE, CAR_MASK_POSITION, CAR_MASK_HEADLIGHT };
    uint8_t file[3] = { 7, 8, 9 };
    LampTexels l = { 0 };
    lamp_texels_collect(&l, 0, mask, 3);
    lamp_texels_read(&l, file, sizeof file);
    const LampEntries e = { 0, 0xf0, 0, 0 };   /* no free entry for the lit brake or the headlight */
    CHECK_EQ_INT(lamp_texels_write(&l, file, sizeof file, LAMP_BRAKING, &e), 1);
    CHECK_EQ_INT(file[0], 7);
    CHECK_EQ_INT(file[1], 0xf0);
    CHECK_EQ_INT(file[2], 9);
    lamp_texels_free(&l);
}

static void test_low_light(void)
{
    const float day[3] = { 1, 1, 1 }, dusk[3] = { 0.88f, 0.55f, 0.46f }, tunnel[3] = { 0.5f, 0.5f, 0.5f }, brighter[3] = { 1.4f, 1.0f, 2.0f }, rounded[3] = { 0.996f, 1, 1 };
    CHECK(!lamp_emissive_low(day, day));
    CHECK(!lamp_emissive_low(NULL, NULL));
    CHECK(lamp_emissive_low(dusk, day));      /* dawn and dusk */
    CHECK(lamp_emissive_low(day, tunnel));    /* a tunnel's or shade's ratio */
    CHECK(!lamp_emissive_low(day, brighter)); /* a light above 1 is not low */
    CHECK(!lamp_emissive_low(rounded, NULL)); /* the day's rounding */
}

static void test_shade_does_not_light_the_lamps(void)
{
    const float day[3] = { 1, 1, 1 }, shade[3] = { 0.9f, 0.9f, 0.9f }, dimmer[3] = { 0.75f, 0.75f, 0.75f }, owners[3] = { 0.875f, 0.875f, 0.875f }, tunnel[3] = { 0.5f, 0.5f, 0.5f },
                dusk[3] = { 0.88f, 0.55f, 0.46f };
    LampLight l = { 0, 0 };
    CHECK(!lamp_light_step(&l, day, shade));   /* a light shade by day: the lamps stay off */
    CHECK(!lamp_light_step(&l, day, dimmer));
    CHECK(lamp_light_step(&l, day, tunnel));   /* a tunnel lights them at once */
    CHECK(lamp_light_step(&l, day, dimmer));   /* between the two levels they stay as they were */
    CHECK(lamp_light_step(&l, day, shade));
    for (int i = 0; i < LAMP_SHADE_HOLD - 2; i++) CHECK(lamp_light_step(&l, day, day));   /* the light has to hold before they go out */
    CHECK(lamp_light_step(&l, day, tunnel));   /* a dip resets the count */
    LampLight t = { 1, 0 };   /* TR3's shaded run (0.875 throughout) after a tunnel: the lamps go out once it holds */
    for (int i = 0; i < LAMP_SHADE_HOLD - 1; i++) CHECK(lamp_light_step(&t, day, owners));
    CHECK(!lamp_light_step(&t, day, owners));
    for (int i = 0; i < LAMP_SHADE_HOLD - 1; i++) CHECK(lamp_light_step(&l, day, day));
    CHECK(!lamp_light_step(&l, day, day));
    CHECK(lamp_light_step(&l, dusk, day));     /* the world's own dawn, dusk and night need no hold */
    CHECK(lamp_light_step(&l, dusk, NULL));
}

int main(void)
{
    test_collect_and_toggle();
    test_reversing_light_lit_only_in_reverse();
    test_four_entries();
    test_offsets_past_the_file_are_dropped();
    test_no_lamps_no_change();
    test_pick_entries();
    test_entry_zero_is_none();
    test_low_light();
    test_shade_does_not_light_the_lamps();
    return TEST_REPORT();
}
