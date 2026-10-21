/* Putting the extras back where the original clears them: the flags after the game's own resets, and reading
 * the flags back into the PortExtras after a code is typed. */
#include "game/frontend/extras_apply.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* port_extras_load / save reach for the platform's storage; nothing here loads or saves, so a file that is
 * never there is all the test needs. */
uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    (void)name, (void)size;
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}

void platform_free_file(uint8_t *data) { free(data); }

/* The original's flags, as central_glue.c hands them over. */
static int32_t unlock_m, unlock_r, variant_m, variant_r, explode, nitro, vegas_cheat, lost_vegas;
static int16_t warrior;
static uint32_t track_flags;

static void flags(ExtrasFlags *f)
{
    f->unlock_m = &unlock_m;
    f->unlock_r = &unlock_r;
    f->variant_m = &variant_m;
    f->variant_r = &variant_r;
    f->explode = &explode;
    f->nitro = &nitro;
    f->vegas_cheat = &vegas_cheat;
    f->lost_vegas = &lost_vegas;
    f->warrior = &warrior;
    f->track_flags = &track_flags;
}

static void clear_all(void)
{
    unlock_m = unlock_r = variant_m = variant_r = explode = nitro = vegas_cheat = lost_vegas = warrior = 0;
    track_flags = 0;
}

static PortExtras extras_of(int mirror, int rally)
{
    PortExtras e = port_extras_defaults();
    e.mirror = mirror;
    e.rally = rally;
    return e;
}

#define WITH_EXTRAS(mirror, rally) const PortExtras e_ = extras_of(mirror, rally), *const e = &e_

/* 00432ff4 and the tournament clear the variants; the switch brings mirror back, rally stays out. */
static void test_mirror_comes_back_alone(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    {
        WITH_EXTRAS(1, 0);
        extras_reapply(e, &f);
    }
    CHECK_EQ_INT(unlock_m, 1);
    CHECK_EQ_INT(variant_m, 1);
    CHECK_EQ_INT(variant_r, 0);
    CHECK_EQ_INT(unlock_r, 0);
}

static void test_rally_comes_back_alone(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    {
        WITH_EXTRAS(0, 1);
        extras_reapply(e, &f);
    }
    CHECK_EQ_INT(unlock_r, 1);
    CHECK_EQ_INT(variant_r, 1);
    CHECK_EQ_INT(variant_m, 0);
    CHECK_EQ_INT(unlock_m, 0);
}

/* Both on in PortExtras is not reachable from the screen; mirror wins, as in cheat_codes. */
static void test_both_on_takes_mirror(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    {
        WITH_EXTRAS(1, 1);
        extras_reapply(e, &f);
    }
    CHECK_EQ_INT(variant_m, 1);
    CHECK_EQ_INT(variant_r, 0);
}

static void test_turbo_only_touches_bit_8(void)
{
    ExtrasFlags f;
    clear_all();
    track_flags = 0x0405;                       /* other bits already set by the game */
    PortExtras e = port_extras_defaults();
    e.turbo = 1;
    flags(&f);
    extras_reapply(&e, &f);
    CHECK_EQ_INT(track_flags, 0x0405 | 8);
    CHECK_EQ_INT(unlock_m, 0);
    CHECK_EQ_INT(unlock_r, 0);
    CHECK_EQ_INT(explode, 0);
    CHECK_EQ_INT(nitro, 0);
    CHECK_EQ_INT(vegas_cheat, 0);
    CHECK_EQ_INT(lost_vegas, 0);
    CHECK_EQ_INT(warrior, 0);
}

static void test_all_flags_off_changes_nothing(void)
{
    ExtrasFlags f;
    clear_all();
    track_flags = 0x0004;                       /* No Mercy, the game's own */
    flags(&f);
    const PortExtras none = port_extras_defaults();
    extras_reapply(&none, &f);
    CHECK_EQ_INT(track_flags, 0x0004);
    CHECK_EQ_INT(unlock_m, 0);
    CHECK_EQ_INT(unlock_r, 0);
    CHECK_EQ_INT(variant_m, 0);
    CHECK_EQ_INT(variant_r, 0);
    CHECK_EQ_INT(explode, 0);
    CHECK_EQ_INT(nitro, 0);
    CHECK_EQ_INT(vegas_cheat, 0);
    CHECK_EQ_INT(lost_vegas, 0);
    CHECK_EQ_INT(warrior, 0);
}

static void test_reapply_is_idempotent(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    PortExtras e = extras_of(1, 0);
    e.nitro = 1;
    e.warrior = 1;
    extras_reapply(&e, &f);
    const int32_t m = unlock_m, r = unlock_r, vm = variant_m, vr = variant_r;
    extras_reapply(&e, &f);
    CHECK_EQ_INT(unlock_m, m);
    CHECK_EQ_INT(unlock_r, r);
    CHECK_EQ_INT(variant_m, vm);
    CHECK_EQ_INT(variant_r, vr);
    CHECK_EQ_INT(nitro, 1);
}

/* The cheat codes write straight into the flags; reading back gives the screen what to show. */
static void test_from_flags_after_a_typed_code(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    variant_m = 1;                              /* SKCART just typed */
    const PortExtras e = extras_from_flags(&(PortExtras){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, START_GAME_MODE }, &f);
    CHECK_EQ_INT(e.mirror, 1);
    CHECK_EQ_INT(e.rally, 0);
}

static void test_from_flags_rally_wins_over_mirror(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    unlock_r = 1;
    variant_r = 1;
    const PortExtras e = extras_from_flags(&(PortExtras){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, START_GAME_MODE }, &f);
    CHECK_EQ_INT(e.rally, 1);
    CHECK_EQ_INT(e.mirror, 0);
}

static void test_from_flags_reads_the_rest(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    explode = 1;
    nitro = 1;
    track_flags = 8;
    warrior = 1;
    vegas_cheat = 1;
    const PortExtras e = extras_from_flags(&(PortExtras){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, START_GAME_MODE }, &f);
    CHECK_EQ_INT(e.explode, 1);
    CHECK_EQ_INT(e.nitro, 1);
    CHECK_EQ_INT(e.turbo, 1);
    CHECK_EQ_INT(e.warrior, 1);
    CHECK_EQ_INT(e.vegas, 1);
    CHECK_EQ_INT(port_extras_cheat_marks(&e), 7);
}

static void test_from_flags_keeps_the_untouched_switches(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    PortExtras e = extras_of(0, 0);
    e.no_movies = 1;
    e.replay_cameras = 1;
    e.start = START_NETWORK;
    const PortExtras r = extras_from_flags(&e, &f);
    CHECK_EQ_INT(r.no_movies, 1);
    CHECK_EQ_INT(r.replay_cameras, 1);
    CHECK_EQ_INT(r.start, START_NETWORK);
    CHECK_EQ_INT(r.mirror, 0);
}

/* the options screen's ABS_LIT order: 0 On, 1 Off, 2 N/A */
static void test_no_assists_turns_the_aids_off_and_leaves_not_offered(void)
{
    int16_t abs = 0, traction = 1;
    extras_no_assists(&abs, &traction);
    CHECK_EQ_INT(abs, 1);        /* on -> off */
    CHECK_EQ_INT(traction, 1);   /* off stays off */
    abs = 2, traction = 2;
    extras_no_assists(&abs, &traction);
    CHECK_EQ_INT(abs, 2);        /* a car without them stays N/A */
    CHECK_EQ_INT(traction, 2);
    abs = 2, traction = 0;
    extras_no_assists(&abs, &traction);
    CHECK_EQ_INT(abs, 2);
    CHECK_EQ_INT(traction, 1);
    abs = 1, traction = 1;
    extras_no_assists(&abs, &traction);   /* idempotent */
    CHECK_EQ_INT(abs, 1);
    CHECK_EQ_INT(traction, 1);
}

int main(void)
{
    test_no_assists_turns_the_aids_off_and_leaves_not_offered();
    test_mirror_comes_back_alone();
    test_rally_comes_back_alone();
    test_both_on_takes_mirror();
    test_turbo_only_touches_bit_8();
    test_all_flags_off_changes_nothing();
    test_reapply_is_idempotent();
    test_from_flags_after_a_typed_code();
    test_from_flags_rally_wins_over_mirror();
    test_from_flags_reads_the_rest();
    test_from_flags_keeps_the_untouched_switches();
    return TEST_REPORT();
}