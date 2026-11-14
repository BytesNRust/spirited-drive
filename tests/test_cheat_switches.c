/* The cheat codes typed in Control Central (central_keys.c's cheat_codes, through central_cheats) and the EXTRAS
 * switches they show afterwards: each code turns its own switch on, read back the way extras_reflect saves it
 * (extras_from_flags_port over the flags), and the saved switches come back on the next visit (extras_reapply_port).
 * cheat_step is the test's own (the letters' spelling is central_glue.c's table), so this checks the mapping from a
 * code to its switch, not the strings. */
#include "game/frontend/central.h"
#include "game/frontend/central_run.h"
#include "game/frontend/extras_apply.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

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

/* the flags Control Central holds, as the state points at them */
typedef struct {
    int16_t race_type, track, player_car, lost, unlock_level, letter_held, warrior, nomercy;
    int32_t unlock_m, unlock_r, lost_vegas, vegas_cheat, variant_m, variant_r, explode, nitro;
    uint32_t track_flags;
    uint8_t keys[256];
} Flags;

static int typed;   /* the code the player types this time: its CENTRAL_* index */

static int32_t scancode(void *ctx, int32_t c) { (void)ctx; return c; }
static int32_t cheat_step(void *ctx, int which) { (void)ctx; return which == typed; }
static void sound(void *ctx, int32_t s) { (void)ctx, (void)s; }

static void bind(ControlCentralState *st, Flags *f)
{
    memset(st, 0, sizeof *st);
    st->race_type = &f->race_type, st->track = &f->track, st->player_car = &f->player_car;
    st->lost = f->lost, st->unlock_level = &f->unlock_level, st->letter_held = &f->letter_held;
    st->unlock_m = &f->unlock_m, st->unlock_r = &f->unlock_r, st->lost_vegas = &f->lost_vegas;
    st->vegas_cheat = &f->vegas_cheat, st->variant_m = &f->variant_m, st->variant_r = &f->variant_r;
    st->explode = &f->explode, st->nitro = &f->nitro, st->warrior = &f->warrior, st->nomercy = &f->nomercy;
    st->track_flags = &f->track_flags, st->keys = f->keys;
}

static ExtrasFlags flags_of(Flags *f)
{
    ExtrasFlags e = { &f->unlock_m, &f->unlock_r, &f->variant_m, &f->variant_r, &f->explode, &f->nitro,
                      &f->vegas_cheat, &f->lost_vegas, &f->warrior, &f->track_flags };
    return e;
}

/* One key pressed: the letter A held, the code's step taken, then the letter let go. */
static void type(ControlCentralState *st, Flags *f, int code)
{
    static const ControlCentralOps ops = { .scancode = scancode, .cheat_step = cheat_step, .sound = sound };
    CentralRun r;
    memset(&r, 0, sizeof r);
    r.st = st, r.ops = &ops;
    typed = code;
    f->keys['A'] = 1;
    f->letter_held = 0;
    central_cheats(&r);
    f->keys['A'] = 0;
}

static void fresh(Flags *f, ControlCentralState *st)
{
    memset(f, 0, sizeof *f);
    bind(st, f);
}

/* the switches as extras.cfg would hold them after the codes typed */
static PortExtras switches_of(Flags *f)
{
    const PortExtras before = port_extras_defaults();
    ExtrasFlags e = flags_of(f);
    return extras_from_flags_port(&before, &e);
}

/* one code typed on a fresh Control Central: the switches it turns on, in the order of PortExtras' fields used here */
static void check_only(int code, int mirror, int rally, int explode, int nitro, int vegas, int warrior, int turbo)
{
    Flags f;
    ControlCentralState st;
    fresh(&f, &st);
    type(&st, &f, code);
    const PortExtras s = switches_of(&f);
    CHECK_EQ_INT(s.mirror, mirror);
    CHECK_EQ_INT(s.rally, rally);
    CHECK_EQ_INT(s.explode, explode);
    CHECK_EQ_INT(s.nitro, nitro);
    CHECK_EQ_INT(s.vegas, vegas);
    CHECK_EQ_INT(s.warrior, warrior);
    CHECK_EQ_INT(s.turbo, turbo);
}

static void test_skcart_is_the_mirror_switch(void) { check_only(CENTRAL_SKCART, 1, 0, 0, 0, 0, 0, 0); }

static void test_earallye_is_the_rally_switch(void) { check_only(CENTRAL_EARALLYE, 0, 1, 0, 0, 0, 0, 0); }

static void test_explode_nitro_casinovegas_turbo_each_their_own(void)
{
    check_only(CENTRAL_EXPLODE, 0, 0, 1, 0, 0, 0, 0);
    check_only(CENTRAL_NITRO, 0, 0, 0, 1, 0, 0, 0);
    check_only(CENTRAL_CASINOVILLE, 0, 0, 0, 0, 1, 0, 0);
    check_only(CENTRAL_TURBO, 0, 0, 0, 0, 0, 0, 1);
}

/* CONCEPTCAR is the Warrior: its switch, and the Warrior as the player's car (11) */
static void test_conceptcar_is_the_warrior_switch(void)
{
    Flags f;
    ControlCentralState st;
    fresh(&f, &st);
    type(&st, &f, CENTRAL_CONCEPTCAR);
    CHECK_EQ_INT(f.player_car, 11);
    CHECK_EQ_INT(switches_of(&f).warrior, 1);
    CHECK_EQ_INT(switches_of(&f).mirror, 0);
}

/* ROCKYROAD steps the unlock level: 1 gives the Warrior and the mirror's unlock, 2 the rally's */
static void test_rockyroad_steps_the_unlock_level(void)
{
    Flags f;
    ControlCentralState st;
    fresh(&f, &st);
    type(&st, &f, CENTRAL_ROCKYROAD);
    CHECK_EQ_INT(f.unlock_level, 1);
    CHECK_EQ_INT(f.warrior, 1);
    CHECK_EQ_INT(f.unlock_m, 1);
    CHECK_EQ_INT(f.unlock_r, 0);
    type(&st, &f, CENTRAL_ROCKYROAD);
    CHECK_EQ_INT(f.unlock_level, 2);
    CHECK_EQ_INT(f.unlock_r, 1);
}

/* the codes typed, saved as the switches, come back on the next visit (extras_reapply_port) */
static void test_typed_codes_come_back_on_the_next_visit(void)
{
    Flags f;
    ControlCentralState st;
    fresh(&f, &st);
    type(&st, &f, CENTRAL_SKCART);
    type(&st, &f, CENTRAL_NITRO);
    const PortExtras saved = switches_of(&f);
    Flags next;
    ControlCentralState st2;
    fresh(&next, &st2);
    ExtrasFlags e = flags_of(&next);
    extras_reapply_port(&saved, &e);
    CHECK_EQ_INT(next.variant_m, 1);
    CHECK_EQ_INT(next.unlock_m, 1);
    CHECK_EQ_INT(next.nitro, 1);
    CHECK_EQ_INT(next.variant_r, 0);
}

int main(void)
{
    test_skcart_is_the_mirror_switch();
    test_earallye_is_the_rally_switch();
    test_explode_nitro_casinovegas_turbo_each_their_own();
    test_conceptcar_is_the_warrior_switch();
    test_rockyroad_steps_the_unlock_level();
    test_typed_codes_come_back_on_the_next_visit();
    return TEST_REPORT();
}
