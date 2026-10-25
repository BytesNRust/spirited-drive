/* Control Central's CHEATS ENABLED banner: on while a cheat that sends the race's records to the cheats table is on
 * (the records routing's own predicate), drawn through the ops, and redrawn when the flags change. */
#include "game/frontend/central_banner.h"
#include "game/frontend/records_route.h"
#include "test.h"

#include <string.h>

/* central_banner.c reaches records_route.c and its port tables, which read files through the platform; the test has no
 * files */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

typedef struct {
    int calls;
    char text[0x40];
    int32_t style, x, y;
} Drawn;

static void plus_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Drawn *d = ctx;
    d->calls++;
    snprintf(d->text, sizeof d->text, "%s", text);
    d->style = style, d->x = x, d->y = y;
}

/* the flags Control Central holds, as the state points at them */
typedef struct {
    int32_t explode, nitro, variant_m, variant_r;
    uint32_t track_flags;
    int16_t car;
} Flags;

static void bind(ControlCentralState *st, Flags *f)
{
    memset(st, 0, sizeof *st);
    st->explode = &f->explode, st->nitro = &f->nitro, st->variant_m = &f->variant_m, st->variant_r = &f->variant_r;
    st->track_flags = &f->track_flags, st->player_car = &f->car;
}

static Flags none(void) { return (Flags){ 0, 0, 0, 0, 0, 7 }; }

static void test_predicate(void)
{
    ControlCentralState st;
    Flags f = none();
    bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 0);

    f = none(), f.explode = 1, bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 1);
    f = none(), f.nitro = 1, bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 1);
    f = none(), f.track_flags = 8, bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 1);
    f = none(), f.car = 11, bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 1);

    /* mirror and rally are variants, not cheats; No Mercy (4) is not one either; the other cars are not */
    f = none(), f.variant_m = 1, f.variant_r = 1, bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 0);
    f = none(), f.track_flags = 4, bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 0);
    f = none(), f.car = 7, bind(&st, &f);
    CHECK_EQ_INT(central_cheats_on(&st), 0);
}

/* the banner's flag is the routing's: the same kind, the same answer, for every mix of the flags */
static void test_agrees_with_routing(void)
{
    for (int bits = 0; bits < 32; bits++) {
        Flags f = none();
        f.explode = bits & 1, f.nitro = (bits >> 1) & 1, f.track_flags = (bits >> 2) & 1 ? 8 : 0;
        f.variant_m = (bits >> 3) & 1, f.variant_r = (bits >> 4) & 1;
        f.car = (bits & 0x10) ? 11 : 0;
        ControlCentralState st;
        bind(&st, &f);
        const RaceKind kind = records_kind_with_car(
            records_race_kind(f.variant_m, f.variant_r, f.explode, f.nitro, (f.track_flags & 8) != 0), f.car);
        CHECK_EQ_INT(central_cheats_on(&st), records_has_cheats(kind));
    }
}

static void test_draw(void)
{
    ControlCentralOps ops;
    CentralRun r;
    Drawn d = { 0 };
    ControlCentralState st;
    Flags f = none();
    memset(&ops, 0, sizeof ops);
    ops.ctx = &d, ops.plus_text = plus_text, ops.port_banner = 1;
    bind(&st, &f);
    memset(&r, 0, sizeof r);
    r.st = &st, r.ops = &ops;

    central_banner(&r);
    CHECK_EQ_INT(d.calls, 0);   /* off: nothing drawn */
    CHECK_EQ_INT(r.banner_shown, 0);

    f.nitro = 1, bind(&st, &f);
    central_banner(&r);
    CHECK_EQ_INT(d.calls, 1);
    CHECK(strcmp(d.text, "CHEATS ENABLED") == 0);
    CHECK_EQ_INT(d.style, 2);   /* TEXT_RED: not the cream of the selectable items */
    CHECK_EQ_INT(r.banner_shown, 1);
}

static void test_stale(void)
{
    ControlCentralOps ops;
    CentralRun r;
    ControlCentralState st;
    Flags f = none();
    memset(&ops, 0, sizeof ops);
    memset(&r, 0, sizeof r);
    bind(&st, &f);
    ops.port_banner = 1;
    r.st = &st, r.ops = &ops;

    CHECK_EQ_INT(central_banner_stale(&r), 0);   /* off, not shown */
    f.car = 11, bind(&st, &f);
    CHECK_EQ_INT(central_banner_stale(&r), 1);   /* the Warrior picked: draw again */
    r.banner_shown = 1;
    CHECK_EQ_INT(central_banner_stale(&r), 0);   /* shown and still on */
    f.car = 0, bind(&st, &f);
    CHECK_EQ_INT(central_banner_stale(&r), 1);   /* the car changed back: the banner goes */
    r.banner_shown = 0;
    CHECK_EQ_INT(central_banner_stale(&r), 0);
}

/* flag 0 is the exe: with every cheat on, nothing is drawn and the banner never reads as stale, so turn() never sets
 * redraw from it (the oracle's central runs this path) */
static void test_flag_off_is_the_exe(void)
{
    ControlCentralOps ops;
    CentralRun r;
    Drawn d = { 0 };
    ControlCentralState st;
    Flags f = none();
    f.explode = 1, f.nitro = 1, f.track_flags = 8, f.car = 11;
    memset(&ops, 0, sizeof ops);
    ops.ctx = &d, ops.plus_text = plus_text;   /* port_banner stays 0 */
    bind(&st, &f);
    memset(&r, 0, sizeof r);
    r.st = &st, r.ops = &ops;
    CHECK_EQ_INT(central_cheats_on(&st), 1);
    CHECK_EQ_INT(central_banner_stale(&r), 0);
    central_banner(&r);
    CHECK_EQ_INT(d.calls, 0);
    CHECK_EQ_INT(r.banner_shown, 0);
    CHECK_EQ_INT(central_banner_stale(&r), 0);   /* still, so no redraw is asked for */
}

int main(void)
{
    test_predicate();
    test_agrees_with_routing();
    test_draw();
    test_stale();
    test_flag_off_is_the_exe();
    return TEST_REPORT();
}
