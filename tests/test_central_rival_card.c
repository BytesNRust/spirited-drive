/* A click on the rival's card in Control Central (central_select.c): the gallery opens on the rival's car, the field
 * (8) showing car 0 as the original does. The gallery has only the 8 cars (showcase.c), so a rival past them, the
 * Warrior (11) the port's Warrior-as-a-rival extra offers or a race type's 9 and 10, leaves the card doing nothing,
 * as the player's own card does with the Warrior. */
#include "game/frontend/central_run.h"
#include "formats/edition_ini.h"
#include "game/frontend/showcase.h"
#include "game/race/car_registry.h"

#include <stdio.h>
#include <string.h>
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

/* dep59 step 3, Task 9: the player's own card with an added car picked opens its own gallery (added_gallery), never the 8 cars' read past
 * their names; when its Previous / Next go to an original, that car's gallery opens; the Warrior's (none) leaves to Control Central */
static void add(const char *id, const char *stem);
static int added_shown, added_to;
static int32_t added_gallery(void *ctx, int16_t *car, int warrior, const uint8_t palette[768])
{
    (void)ctx, (void)warrior, (void)palette;
    added_shown++;
    if (added_to < 0) return 0;
    *car = (int16_t)added_to;
    return 1;
}

static void test_an_added_car_card_opens_nothing(void)
{
    static const uint8_t pal[0x20];
    ControlCentralOps o = { 0 };
    o.pointer_click = nothing, o.sound = sound, o.in_rect = outside, o.music_fade_out = nothing, o.fade_out = fade_out;
    o.music_end = nothing, o.music_start = music_start, o.showcase = gallery, o.showcase_after = nothing;
    o.palette_put = palette_put, o.pointer_move_to = move_to, o.car_flags = nothing;
    ControlCentralState st = { 0 };
    static int16_t warrior;
    st.player_car = &player_car, st.opponent = &opponent, st.race_type = &race_type, st.net_second = &net_second, st.was = was;
    st.warrior = &warrior;
    CentralRun r = { 0 };
    r.st = &st, r.ops = &o, r.pal = pal, r.item = CC_CAR, r.keys_armed = 1;
    car_registry_clear();
    add("a", "A1");   /* 16 */
    player_car = 16, opponent = 3, shown = 0, added_shown = 0, added_to = -1;
    central_select(&r);   /* no op: nothing opens */
    CHECK_EQ_INT(shown, 0);
    CHECK_EQ_INT(player_car, 16);
    o.added_gallery = added_gallery;
    central_select(&r);   /* its own page, left with Done */
    CHECK(added_shown == 1 && shown == 0 && player_car == 16);
    added_to = 2;          /* Next went round to an original: that car's gallery */
    central_select(&r);
    CHECK(added_shown == 2 && shown == 1 && player_car == 2);
    player_car = 16, added_to = 11;   /* to the Warrior: back to Control Central, the Warrior picked */
    central_select(&r);
    CHECK(added_shown == 3 && shown == 1 && player_car == 11);
    player_car = 4;
    central_select(&r);
    CHECK_EQ_INT(shown, 2);   /* an original's still opens the 8 cars' */
    CHECK_EQ_INT(added_shown, 3);
    car_registry_clear();
}

/* dep59 step 3, Task 8: an added car's card and rival card come from its pictures (load_added_card), never from a bank of the CD's;
 * Review Focus 1: the fifth added car (20) is loaded again after the no-art mark, as any other */
static int loads, added_loads, added_car, added_rival, blits, frees;
static int32_t load_bank(void *ctx, const char *path, int32_t flags) { (void)ctx, (void)path, (void)flags; return loads++, 5; }
static int32_t load_added(void *ctx, int car, int rival, const uint8_t palette[768])
{
    (void)ctx, (void)palette;
    added_loads++, added_car = car, added_rival = rival;
    return 9;
}
static const uint8_t *shape(void *ctx, int32_t bank, const char *tag) { (void)ctx, (void)bank, (void)tag; static const uint8_t s[16]; return s; }
static void blit(void *ctx, const uint8_t *s) { (void)ctx, (void)s; blits++; }
static void art_free(void *ctx, int32_t handle) { (void)ctx, (void)handle; frees++; }

static void cards(int16_t player, int16_t rival, int16_t was_car, int16_t was_rival)
{
    static int32_t art[8];
    static const uint8_t pal[0x20];
    static int16_t net_self, net_car;
    ControlCentralOps o = { 0 };
    o.load_bank = load_bank, o.load_added_card = load_added, o.shape = shape, o.blit = blit, o.free = art_free;
    ControlCentralState st = { 0 };
    st.player_car = &player_car, st.opponent = &opponent, st.race_type = &race_type, st.net_self = &net_self, st.net_car = &net_car;
    st.was = was, st.art = art, st.paths = "";
    CentralRun r = { 0 };
    r.st = &st, r.ops = &o, r.pal = pal;
    player_car = player, opponent = rival, was[WAS_CAR] = was_car, was[WAS_OPPONENT] = was_rival;
    loads = added_loads = blits = frees = 0, added_car = added_rival = -1;
    central_car_art(&r);
    central_opponent_art(&r);
}

static void add(const char *id, const char *stem)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.dash_model = -1;
    CHECK(car_registry_add(&p, NULL, 0) >= CAR_ADDED_FIRST);
}

static void test_added_cards(void)
{
    car_registry_clear();
    add("a", "A1"), add("b", "B1"), add("c", "C1"), add("d", "D1"), add("e", "E1");   /* 16..20 */
    race_type = 1;
    cards(16, 3, CENTRAL_NO_ART, CENTRAL_NO_ART);
    CHECK_EQ_INT(added_loads, 1);
    CHECK(added_car == 16 && added_rival == 0);
    CHECK_EQ_INT(loads, 1);   /* the rival's, an original's */
    CHECK_EQ_INT(blits, 2);
    cards(3, 17, CENTRAL_NO_ART, CENTRAL_NO_ART);
    CHECK_EQ_INT(added_loads, 1);
    CHECK(added_car == 17 && added_rival == 1);
    cards(3, 20, 3, CENTRAL_NO_ART);   /* Review Focus 1: the fifth added car, its card loaded again */
    CHECK_EQ_INT(added_loads, 1);
    CHECK(added_car == 20 && added_rival == 1);
    CHECK_EQ_INT(loads, 0);   /* the player's card was there already */
    cards(3, 20, 3, 20);      /* nothing changed: nothing loaded */
    CHECK_EQ_INT(added_loads + loads, 0);
    cards(40, 3, CENTRAL_NO_ART, 3);   /* a number not registered: no card, nothing read past a table; the rival's as it was */
    CHECK_EQ_INT(added_loads + loads, 0);
    CHECK_EQ_INT(blits, 1);
    car_registry_clear();
}

/* dep59 section 4 (the owner's carousel): in Restomod outside a tournament the player's card opens one carousel over the edition's lineup.
 * An original's showcase leaves on its Next / Previous (showcase_carousel), an added car's page hands back on an original (added_gallery),
 * and the next car in the edition's order (wrapping, the Warrior passed over: no page yet) opens; Done ends on the car shown, picked. In
 * a tournament, with no edition, or lost, the original's showcase as it was */
static int carousel_calls, plain_calls, seen_count;
static int16_t seen[16];
static const int *steps;      /* the showcases' answers in turn: 1 Next, -1 Previous, 0 Done */
static const int16_t *hands;  /* the added pages' in turn: the original they hand back on, or -1 for Done */
static int step_at, hand_at;
static void see(char kind, int16_t car)
{
    if (seen_count < 16) seen[seen_count++] = (int16_t)(kind == 'S' ? car : 100 + car);
}
static int32_t carousel_show(void *ctx) { (void)ctx, carousel_calls++, see('S', player_car); return steps[step_at++]; }
static void plain_show(void *ctx) { (void)ctx, plain_calls++, see('S', player_car); }
static int32_t page(void *ctx, int16_t *car, int warrior, const uint8_t palette[768])
{
    (void)ctx, (void)warrior, (void)palette;
    see('A', *car);
    const int16_t to = hands[hand_at++];
    if (to < 0) return 0;
    *car = to;
    return 1;
}

static void carousel(const char *cars, int16_t type, int16_t from, const int *s, const int16_t *h)
{
    static const uint8_t pal[0x20];
    static int16_t warrior = 1, rival = 3, kind;
    static Edition e;
    char text[256], why[64];
    snprintf(text, sizeof text, "name=Walk\ncars=%s\ntracks=city\n", cars);
    CHECK(edition_parse((const uint8_t *)text, strlen(text), &e, why, sizeof why));
    ControlCentralOps o = { 0 };
    o.pointer_click = nothing, o.sound = sound, o.in_rect = outside, o.music_fade_out = nothing, o.fade_out = fade_out;
    o.music_end = nothing, o.music_start = music_start, o.showcase = plain_show, o.showcase_after = nothing;
    o.palette_put = palette_put, o.pointer_move_to = move_to, o.car_flags = nothing;
    o.added_gallery = page, o.showcase_carousel = carousel_show;
    ControlCentralState st = { 0 };
    kind = type;
    st.player_car = &player_car, st.opponent = &rival, st.race_type = &kind, st.net_second = &net_second, st.was = was;
    st.warrior = &warrior, st.edition = cars[0] != 0 ? &e : NULL;
    CentralRun r = { 0 };
    r.st = &st, r.ops = &o, r.pal = pal, r.item = CC_CAR, r.keys_armed = 1;
    player_car = from, steps = s, hands = h, step_at = hand_at = carousel_calls = plain_calls = seen_count = 0;
    central_select(&r);
}

static void test_the_carousel(void)
{
    car_registry_clear();
    add("a", "A1");   /* 16 */
    {   /* the Diablo, Next to the added car, whose Next hands back on the RX-7, Previous from it to the added car again, Done there */
        static const int s[] = { 1, -1 };
        static const int16_t h[] = { 0, -1 };
        carousel("diablo,a,rx7", 1, 7, s, h);
        static const int16_t want[] = { 7, 116, 0, 116 };
        CHECK_EQ_INT(seen_count, 4);
        CHECK(memcmp(seen, want, sizeof want) == 0);
        CHECK_EQ_INT(player_car, 16);
        CHECK_EQ_INT(plain_calls, 0);
    }
    {   /* Previous from the first wraps round to the last; Done there */
        static const int s[] = { -1, 0 };
        carousel("diablo,a,rx7", 1, 7, s, NULL);
        CHECK(seen_count == 2 && seen[0] == 7 && seen[1] == 0);
        CHECK_EQ_INT(player_car, 0);
    }
    {   /* the Warrior has no page yet: passed over */
        static const int s[] = { 1, 0 };
        carousel("diablo,warrior,rx7", 1, 7, s, NULL);
        CHECK(seen_count == 2 && seen[1] == 0);
    }
    {   /* the added car's own card opens the carousel too: its page hands back on an original, Done there */
        static const int s[] = { 0 };
        static const int16_t h[] = { 7 };
        carousel("diablo,a,rx7", 1, 16, s, h);
        CHECK(seen_count == 2 && seen[0] == 116 && seen[1] == 7);
        CHECK_EQ_INT(player_car, 7);
        CHECK_EQ_INT(plain_calls, 0);
    }
    {   /* a tournament: the original's showcase as it was */
        carousel("diablo,a,rx7", 3, 7, NULL, NULL);
        CHECK(plain_calls == 1 && carousel_calls == 0);
    }
    {   /* no edition (Stock, Restored): the original's showcase as it was */
        carousel("", 1, 7, NULL, NULL);
        CHECK(plain_calls == 1 && carousel_calls == 0);
    }
    car_registry_clear();
}

int main(void)
{
    test_the_carousel();
    test_no_art_is_no_car();
    test_added_cards();
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
