/* The reflectors' second pass (reflect_pass.h): which textures are registered, when it is active, the see-through copy, the entries a family takes. */
#include "game/race/reflector.h"
#include "game/viewer/reflect_pass.h"
#include "tests/test.h"

#include <string.h>

enum { W = 8, H = 4, RED_ENTRY = 10, DIM_RED = 9, WHITE_ENTRY = 11, WARM = 20 };

static uint8_t pixels[W * H], mask[W * H], palette[768];
static FshEntry bitmap;

static void setup(void)
{
    reflect_pass_reset();
    memset(palette, 0, sizeof palette);
    palette[3 * 1 + 0] = 200, palette[3 * 1 + 1] = 20, palette[3 * 1 + 2] = 20;        /* index 1: red */
    palette[3 * 2 + 0] = 230, palette[3 * 2 + 1] = 230, palette[3 * 2 + 2] = 230;      /* index 2: white */
    palette[3 * 3 + 0] = 20, palette[3 * 3 + 1] = 140, palette[3 * 3 + 2] = 60;        /* index 3: green */
    for (int i = 0; i < W * H; i++) pixels[i] = (uint8_t)(1 + i % 3);
    memset(mask, 0, sizeof mask);
    mask[0] = mask[1] = mask[2] = 1;
    memset(&bitmap, 0, sizeof bitmap);
    bitmap.width = W, bitmap.height = H, bitmap.payload = pixels, bitmap.payload_size = sizeof pixels, bitmap.type = 0x41;
}

static void test_a_texture_is_registered_once_and_found_by_its_pixels(void)
{
    setup();
    CHECK_EQ_INT(reflect_pass_add(&bitmap, mask, W, H), 1);
    CHECK_EQ_INT(reflect_pass_add(&bitmap, mask, W, H), 0);   /* the same bitmap again: no */
    CHECK_EQ_INT(reflect_pass_count(), 1);
    FshEntry other = bitmap;   /* a copy of the struct is the same bitmap */
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    CHECK(reflect_pass_copy(&other) != NULL);
    uint8_t elsewhere[W * H] = { 0 };
    other.payload = elsewhere;
    CHECK(reflect_pass_copy(&other) == NULL);
}

static void test_a_blank_mask_is_not_registered(void)
{
    setup();
    uint8_t blank[W * H] = { 0 };
    CHECK_EQ_INT(reflect_pass_add(&bitmap, blank, W, H), 0);
    CHECK_EQ_INT(reflect_pass_count(), 0);
    uint8_t small[2 * 2] = { 0, 0, 0, 1 };   /* a smaller mask is resampled to the bitmap's size, any-of */
    CHECK_EQ_INT(reflect_pass_add(&bitmap, small, 2, 2), 1);
}

static void test_the_copy_keeps_the_masked_texels_and_hides_the_rest(void)
{
    setup();
    reflect_pass_add(&bitmap, mask, W, H);
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    const FshEntry *c = reflect_pass_copy(&bitmap);
    CHECK(c != NULL && c->payload != pixels);
    CHECK_EQ_INT(c->width, W);
    CHECK_EQ_INT(c->type, 0x45);   /* the transparent type */
    for (int i = 0; i < W * H; i++) CHECK_EQ_INT(c->payload[i], mask[i] ? pixels[i] : 0xff);
    pixels[0] = 3;   /* the bitmap is remapped in place after a palette change: the copy follows it */
    palette[0] ^= 1;
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    CHECK_EQ_INT(reflect_pass_copy(&bitmap)->payload[0], 3);
}

static void test_the_thinned_copies_keep_half_and_a_quarter_of_the_lit_texels(void)
{
    setup();
    reflect_pass_add(&bitmap, mask, W, H);
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    const FshEntry *c = reflect_pass_copy(&bitmap);
    CHECK(reflect_pass_thin_copy(c, 0) == c);
    const FshEntry *half = reflect_pass_thin_copy(c, 1), *quarter = reflect_pass_thin_copy(c, 2);
    CHECK(half != c && quarter != c && half != quarter);
    CHECK_EQ_INT(half->type, 0x45);
    int lit = 0, h = 0, q = 0;
    for (int i = 0; i < W * H; i++) {
        lit += c->payload[i] != 0xff, h += half->payload[i] != 0xff, q += quarter->payload[i] != 0xff;
        if (half->payload[i] != 0xff) CHECK_EQ_INT(half->payload[i], c->payload[i]);   /* what is kept is the copy's texel */
        if (quarter->payload[i] != 0xff) CHECK_EQ_INT(quarter->payload[i], c->payload[i]);
    }
    CHECK(h <= lit && q <= h);
    CHECK(reflect_pass_thin_copy(NULL, 1) == NULL);
}

static void test_the_pass_runs_only_with_lamps_dark_and_entries(void)
{
    setup();
    reflect_pass_add(&bitmap, mask, W, H);
    const uint8_t street[3] = { WARM + 2, WARM + 1, WARM };
    reflect_pass_set_entries(street, 0, RED_ENTRY, DIM_RED, WHITE_ENTRY, 0);
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    CHECK(reflect_pass_active());
    CHECK(reflect_pass_gain(50, 0, 0.65) > 0);
    reflect_pass_frame(0, 0, 0, 1, 0, 1, palette);   /* the lamps off */
    CHECK(!reflect_pass_active());
    CHECK_EQ_INT(reflect_pass_gain(50, 0, 0.65), 0);
    reflect_pass_frame(1, 0, 0, 1, 0, 0, palette);   /* daylight */
    CHECK(!reflect_pass_active());
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    reflect_pass_set_entries(NULL, 0, 0, 0, 0, 0);   /* the night has nothing lit to shine in */
    CHECK(!reflect_pass_active());
}

static void test_the_gain_follows_the_lamps_heading(void)
{
    setup();
    reflect_pass_add(&bitmap, mask, W, H);
    reflect_pass_set_entries(NULL, WARM, 0, 0, 0, 0);
    reflect_pass_frame(1, 100, 200, 0, 1, 1, palette);   /* heading along +z from (100, 200) */
    CHECK(reflect_pass_gain(100, 260, 0.65) > 0);        /* ahead */
    CHECK_EQ_INT(reflect_pass_gain(100, 150, 0.65), 0);  /* behind */
    CHECK(reflect_pass_gain(100, 260, 0.65) > reflect_pass_gain(106, 260, 0.65));   /* off the axis */
}

static void test_each_family_takes_its_own_ramp_and_the_level_a_step(void)
{
    setup();
    reflect_pass_add(&bitmap, mask, W, H);
    const uint8_t street[3] = { WARM + 2, WARM + 1, WARM };   /* dimmest first: street[2] */
    reflect_pass_set_entries(street, 0, RED_ENTRY, DIM_RED, WHITE_ENTRY, 0);
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    const uint8_t *top = reflect_pass_table(REFLECTOR_LEVELS - 1), *low = reflect_pass_table(1);
    CHECK_EQ_INT(top[1], RED_ENTRY);        /* red: the lit brake at the top ... */
    CHECK_EQ_INT(low[1], DIM_RED);          /* ... the position light at the bottom */
    CHECK_EQ_INT(top[2], WHITE_ENTRY);      /* white */
    CHECK_EQ_INT(top[3], WARM + 2);         /* green shines warm: the street lamp's lens at the top */
    CHECK_EQ_INT(low[3], WARM);             /* the rim at the bottom */
}

/* two tracks in one process: the next track's art stands where the last one did (a Track reused, a freed block taken again), and the pass must hold the second's textures, not the first's */
static TrackArt art;
static uint8_t scenery_pixels[3][W * H];
static const uint8_t *mask_of_wide(const TrackTexture *t) { return t->bitmap.width == W ? mask : NULL; }

static void put_scenery(int index, int board)
{
    memset(scenery_pixels[board], 1 + board, sizeof scenery_pixels[board]);
    TrackTexture *t = &art.scenery[index];
    t->present = 1;
    memset(&t->bitmap, 0, sizeof t->bitmap);
    t->bitmap.width = W, t->bitmap.height = H, t->bitmap.payload = scenery_pixels[board], t->bitmap.payload_size = W * H, t->bitmap.type = 0x41;
}

static void test_the_next_track_at_the_same_address_is_registered_again(void)
{
    setup();
    reflect_pass_forget_art();
    memset(&art, 0, sizeof art);
    put_scenery(0, 0);   /* the first track: one textured board with a mask */
    CHECK_EQ_INT(reflect_pass_register_art(&art, mask_of_wide), 1);
    CHECK_EQ_INT(reflect_pass_register_art(&art, mask_of_wide), -1);   /* the same art: nothing to do */
    memset(&art, 0, sizeof art);   /* the first is freed and the second takes its place (the same address) with three boards */
    put_scenery(1, 1), put_scenery(2, 2);
    CHECK_EQ_INT(reflect_pass_register_art(&art, mask_of_wide), -1);   /* what the old pointer test did: the address is the same, the pass keeps the first's entries */
    reflect_pass_forget_art();   /* what freeing or loading a track does now */
    CHECK_EQ_INT(reflect_pass_register_art(&art, mask_of_wide), 2);
    reflect_pass_frame(1, 0, 0, 1, 0, 1, palette);
    CHECK(reflect_pass_copy(&art.scenery[1].bitmap) != NULL);
    CHECK(reflect_pass_copy(&art.scenery[0].bitmap) == NULL);
    reflect_pass_forget_art();
    CHECK_EQ_INT(reflect_pass_count(), 0);
}

int main(void)
{
    test_a_texture_is_registered_once_and_found_by_its_pixels();
    test_a_blank_mask_is_not_registered();
    test_the_copy_keeps_the_masked_texels_and_hides_the_rest();
    test_the_thinned_copies_keep_half_and_a_quarter_of_the_lit_texels();
    test_the_pass_runs_only_with_lamps_dark_and_entries();
    test_the_gain_follows_the_lamps_heading();
    test_each_family_takes_its_own_ramp_and_the_level_a_step();
    test_the_next_track_at_the_same_address_is_registered_again();
    reflect_pass_reset();
    return TEST_REPORT();
}
