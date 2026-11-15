/* A click on the rival's card in Control Central (central_select.c): the gallery opens on the rival's car, the field
 * (8) showing car 0 as the original does. The gallery has only the 8 cars (showcase.c), so a rival past them, the
 * Warrior (11) the port's Warrior-as-a-rival extra offers or a race type's 9 and 10, leaves the card doing nothing,
 * as the player's own card does with the Warrior. */
#include "game/frontend/central_run.h"
#include "game/frontend/showcase.h"
#include "game/race/car_registry.h"
#include "test.h"

#include "game/platform/platform.h"

/* central_draw.c reaches variant_rule.c, which reads extras.cfg through the platform; the test has no file */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }
bool platform_user_file_exists(const char *name)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data != NULL) platform_free_file(data);
    return data != NULL;
}
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* the gallery's own names (showcase.c CARS), read by the car the gallery is opened on */
static const char *const CARS[SHOWCASE_CARS] = { "rx7", "nsx", "supra", "911", "vette", "viper", "512tr", "diablo" };

static int16_t player_car, opponent, race_type = 1, net_second, was[6];
static int shown;
static const char *shown_name;

static void nothing(void *ctx) { (void)ctx; }
static void sound(void *ctx, int32_t s) { (void)ctx, (void)s; }
static void fade_out(void *ctx, int32_t a, int32_t b) { (void)ctx, (void)a, (void)b; }
static void music_start(void *ctx, const char *name) { (void)ctx, (void)name; }
static void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static void palette_put(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)rgb, (void)first, (void)count, (void)src, (void)mode;
}
static int32_t outside(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)ctx, (void)x0, (void)x1, (void)y0, (void)y1;
    return 0;
}
static void gallery(void *ctx)
{
    (void)ctx;
    shown++;
    shown_name = CARS[player_car];
}

/* the card clicked with this rival; returns the times the gallery opened */
static int click(int16_t rival)
{
    static const uint8_t pal[0x20];
    ControlCentralOps o = { 0 };
    o.pointer_click = nothing, o.sound = sound, o.in_rect = outside, o.music_fade_out = nothing, o.fade_out = fade_out;
    o.music_end = nothing, o.music_start = music_start, o.showcase = gallery, o.showcase_after = nothing;
    o.palette_put = palette_put, o.pointer_move_to = move_to;
    ControlCentralState st = { 0 };
    st.player_car = &player_car, st.opponent = &opponent, st.race_type = &race_type, st.net_second = &net_second, st.was = was;
    CentralRun r = { 0 };
    r.st = &st, r.ops = &o, r.pal = pal, r.item = CC_OPPONENT, r.keys_armed = 1;
    player_car = 3, opponent = rival, shown = 0, shown_name = NULL;
    central_select(&r);
    return shown;
}

/* port (dep59): the opponent's "no art shown" mark (what a click on the card leaves, so the card is loaded again) is no car's number: in
 * the exe it was 0x14, and car 20 is an added car now, whose card would never be loaded again (Review Focus 1) */
static void test_no_art_is_no_car(void)
{
    was[WAS_OPPONENT] = 5;
    click(5);
    CHECK(was[WAS_OPPONENT] != 20);
    CHECK(was[WAS_OPPONENT] < 0 || was[WAS_OPPONENT] >= CAR_ADDED_FIRST + CAR_ADDED_MAX);
}

/* port (dep59): the player's own card with an added car picked opens nothing yet (the gallery has the 8 cars; an added car's own
 * page comes with dep59 step 3's Task 9), never the gallery read past its names */
static void test_an_added_car_card_opens_nothing(void)
{
    static const uint8_t pal[0x20];
    ControlCentralOps o = { 0 };
    o.pointer_click = nothing, o.sound = sound, o.in_rect = outside, o.music_fade_out = nothing, o.fade_out = fade_out;
    o.music_end = nothing, o.music_start = music_start, o.showcase = gallery, o.showcase_after = nothing;
    o.palette_put = palette_put, o.pointer_move_to = move_to;
    ControlCentralState st = { 0 };
    st.player_car = &player_car, st.opponent = &opponent, st.race_type = &race_type, st.net_second = &net_second, st.was = was;
    CentralRun r = { 0 };
    r.st = &st, r.ops = &o, r.pal = pal, r.item = CC_CAR, r.keys_armed = 1;
    player_car = 16, opponent = 3, shown = 0;
    central_select(&r);
    CHECK_EQ_INT(shown, 0);
    CHECK_EQ_INT(player_car, 16);
    player_car = 4;
    central_select(&r);
    CHECK_EQ_INT(shown, 1);   /* an original's still opens it */
}

int main(void)
{
    test_no_art_is_no_car();
    test_an_added_car_card_opens_nothing();
    CHECK_EQ_INT(click(5), 1);
    CHECK(shown_name != NULL && shown_name == CARS[5]);
    CHECK_EQ_INT(player_car, 3);
    CHECK_EQ_INT(opponent, 5);
    CHECK_EQ_INT(click(8), 1);            /* the field: car 0, as the original */
    CHECK(shown_name == CARS[0]);
    CHECK_EQ_INT(opponent, 0);
    for (int16_t rival = 9; rival <= 11; rival++) {
        CHECK_EQ_INT(click(rival), 0);
        CHECK_EQ_INT(player_car, 3);
        CHECK_EQ_INT(opponent, rival);
    }
    return TEST_REPORT();
}
