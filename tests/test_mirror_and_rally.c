/* Mirror and Rally at the same time (port): with both switches on, the race runs the mirror's geometry and the
 * rally's art and track — the FAM "_R01", TR5 in place of TR1 — while the exe keeps Mirror winning whenever the
 * two are combined. Each rule is behind its own port_* flag (0 = the exe, 1 = the port), set per call-site, so the
 * original's answers stay reachable: the cheats in central_keys.c still exclude one from the other, and the
 * records and the track select's entries still follow the original. */
#include "game/frontend/extras_apply.h"
#include "game/frontend/extras_menu.h"
#include "game/frontend/variant_rule.h"
#include "game/race/track_fam.h"
#include "game/sys/port_extras.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* variant_rule_combine() reaches port_extras_load/save, which read / write user/extras.cfg through the
 * platform; the test has no file, so the switches never combine here (every case passes the flag explicitly) */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

/* ----- track_fam.c: 004510e8's name ----- */

/* 004510e8 as the exe: midday with "m" is the mirror's FAM, whichever else is set. */
static void fam_mirror_wins_as_the_exe(void)
{
    const TrackFamInputs in = { .time_of_day = 1, .variant_m = 1, .variant_r = 1, .track = 1,
                                .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&in, 1, name, sizeof name);
    CHECK(strstr(name, "_M01.fam") != NULL);
}

/* the port: both on, midday — the rally's art, not the mirror's. */
static void fam_rally_wins_when_the_port_says_so(void)
{
    const TrackFamInputs in = { .time_of_day = 1, .variant_m = 1, .variant_r = 1, .track = 1, .port_both_variants = 1,
                                .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&in, 1, name, sizeof name);
    CHECK(strstr(name, "_R01.fam") != NULL);
}

/* The flag only speaks when the two are combined: alone, each keeps the exe's own answer. */
static void fam_alone_is_the_exe_with_the_flag_on(void)
{
    const TrackFamInputs m = { .time_of_day = 1, .variant_m = 1, .variant_r = 0, .track = 1, .port_both_variants = 1,
                               .stem = "cl", .dir = "n", .dir_midday = "e" };
    const TrackFamInputs r = { .time_of_day = 1, .variant_m = 0, .variant_r = 1, .track = 1, .port_both_variants = 1,
                               .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&m, 1, name, sizeof name);
    CHECK(strstr(name, "_M01.fam") != NULL);
    track_fam_name(&r, 1, name, sizeof name);
    CHECK(strstr(name, "_R01.fam") != NULL);
}

/* A time of day other than midday is the dawn and dusk art, whatever the variants say. */
static void fam_other_time_keeps_the_t_art(void)
{
    const TrackFamInputs in = { .time_of_day = 0, .variant_m = 1, .variant_r = 1, .track = 1, .port_both_variants = 1,
                                .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&in, 1, name, sizeof name);
    CHECK(strstr(name, "_T01.fam") != NULL);
}

/* Track 3 (Rusty Springs) holds no "r" art, so the rally's branch is skipped for it (004510e8). With the mirror on
 * the name is its "_M" — the exe's answer — and with the rally alone the midday plain: the port's rule keeps both,
 * because there is no TR1_R01 to reach for. */
static void fam_track_three_has_no_rally_art(void)
{
    char name[TRACK_FAM_NAME];
    const TrackFamInputs both = { .time_of_day = 1, .variant_m = 1, .variant_r = 1, .track = 3,
                                  .port_both_variants = 1, .stem = "tr1", .dir = "n", .dir_midday = "e" };
    track_fam_name(&both, 1, name, sizeof name);
    CHECK(strstr(name, "_M01.fam") != NULL);

    TrackFamInputs rally = both;
    rally.variant_m = 0, rally.port_both_variants = 0;
    track_fam_name(&rally, 1, name, sizeof name);
    CHECK(strstr(name, "_001.fam") != NULL);
}

/* ----- extras_apply.c: the flags after the game's own resets ----- */

static int32_t fl_unlock_m, fl_unlock_r, fl_variant_m, fl_variant_r, fl_explode, fl_nitro, fl_vegas, fl_lost_vegas;
static int16_t fl_warrior;
static uint32_t fl_track_flags;

static void flags(ExtrasFlags *f)
{
    memset(f, 0, sizeof *f);
    f->unlock_m = &fl_unlock_m, f->unlock_r = &fl_unlock_r;
    f->variant_m = &fl_variant_m, f->variant_r = &fl_variant_r;
    f->explode = &fl_explode, f->nitro = &fl_nitro;
    f->vegas_cheat = &fl_vegas, f->lost_vegas = &fl_lost_vegas;
    f->warrior = &fl_warrior, f->track_flags = &fl_track_flags;
}

static void clear_all(void)
{
    fl_unlock_m = fl_unlock_r = fl_variant_m = fl_variant_r = 0;
    fl_explode = fl_nitro = fl_vegas = fl_lost_vegas = 0;
    fl_warrior = 0;
    fl_track_flags = 0;
}

static PortExtras both_on(void)
{
    PortExtras e = port_extras_defaults();
    e.mirror = 1, e.rally = 1;
    return e;
}

/* Both switches on: the exe brings back the mirror alone (00432ff4's reset, then cheat_codes' rule). */
static void reapply_both_is_the_exe(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    const PortExtras e = both_on();
    extras_reapply(&e, &f);
    CHECK_EQ_INT(fl_variant_m, 1);
    CHECK_EQ_INT(fl_variant_r, 0);
}

/* The port: both stay on, each unlocked, so the race reads the two together. */
static void reapply_both_keeps_the_two_when_the_port_says_so(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    const PortExtras e = both_on();
    extras_reapply_port(&e, &f);
    CHECK_EQ_INT(fl_unlock_m, 1);
    CHECK_EQ_INT(fl_unlock_r, 1);
    CHECK_EQ_INT(fl_variant_m, 1);
    CHECK_EQ_INT(fl_variant_r, 1);
}

/* Alone each sets its own unlock and variant and leaves the other's unlock alone. */
static void reapply_alone_is_unchanged_by_the_port(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    PortExtras m = port_extras_defaults();
    m.mirror = 1;
    extras_reapply_port(&m, &f);
    CHECK_EQ_INT(fl_unlock_m, 1);
    CHECK_EQ_INT(fl_variant_m, 1);
    CHECK_EQ_INT(fl_variant_r, 0);
    CHECK_EQ_INT(fl_unlock_r, 0);

    clear_all();
    flags(&f);
    PortExtras r = port_extras_defaults();
    r.rally = 1;
    extras_reapply_port(&r, &f);
    CHECK_EQ_INT(fl_unlock_r, 1);
    CHECK_EQ_INT(fl_variant_r, 1);
    CHECK_EQ_INT(fl_variant_m, 0);
    CHECK_EQ_INT(fl_unlock_m, 0);
}

/* The port: one switch forces its variant on and leaves the other as the track screen chose it, so Mirror on and
 * the rally's entry picked races both (the exe's rule cleared the rally and raced the mirror alone). */
static void reapply_alone_keeps_the_picked_variant(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    fl_variant_r = 1;
    PortExtras m = port_extras_defaults();
    m.mirror = 1;
    extras_reapply_port(&m, &f);
    CHECK_EQ_INT(fl_variant_m, 1);
    CHECK_EQ_INT(fl_variant_r, 1);

    clear_all();
    flags(&f);
    fl_variant_m = 1;
    PortExtras r = port_extras_defaults();
    r.rally = 1;
    extras_reapply_port(&r, &f);
    CHECK_EQ_INT(fl_variant_r, 1);
    CHECK_EQ_INT(fl_variant_m, 1);
}

/* Nothing on: the reapply leaves the flags as it found them. */
static void reapply_none_touches_nothing(void)
{
    ExtrasFlags f;
    clear_all();
    fl_track_flags = 4;   /* No Mercy, the game's own */
    flags(&f);
    const PortExtras none = port_extras_defaults();
    extras_reapply_port(&none, &f);
    CHECK_EQ_INT(fl_track_flags, 4);
    CHECK_EQ_INT(fl_variant_m, 0);
    CHECK_EQ_INT(fl_variant_r, 0);
    CHECK_EQ_INT(fl_unlock_m, 0);
    CHECK_EQ_INT(fl_unlock_r, 0);
}

/* Reading the flags back into the switches: both on, the port shows both. The exe's read is still reachable. */
static void from_flags_both_shows_both_under_the_port(void)
{
    ExtrasFlags f;
    clear_all();
    flags(&f);
    fl_variant_m = 1, fl_variant_r = 1;
    const PortExtras r = extras_from_flags_port(&(PortExtras){ 0 }, &f);
    CHECK_EQ_INT(r.mirror, 1);
    CHECK_EQ_INT(r.rally, 1);

    /* the exe's own read: the first branch wins, so a state with both shows mirror */
    const PortExtras e = extras_from_flags(&(PortExtras){ 0 }, &f);
    CHECK_EQ_INT(e.mirror, 1);
    CHECK_EQ_INT(e.rally, 0);
}

/* ----- extras_menu.c: the switches no longer exclude each other under the port ----- */

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, RIGHT = 0x4d00, DOWN = 0x5000 };

typedef struct MenuFake {
    const int *keys;
    int key_count, key_next;
    int saves;
    PortExtras saved;
} MenuFake;

static int32_t mf_key(void *ctx)
{
    MenuFake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static void mf_void(void *ctx) { (void)ctx; }
static void mf_one(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void mf_four(void *ctx, int32_t a, int32_t b, const uint8_t *d)
{
    (void)ctx, (void)a, (void)b, (void)d;
}
static void mf_six(void *ctx, uint8_t *a, int32_t b, int32_t c, const uint8_t *d, int32_t e)
{
    (void)ctx, (void)a, (void)b, (void)c, (void)d, (void)e;
}
static void mf_move(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static int32_t mf_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *mf_shape(void *ctx, int32_t bank, const char *tag)
{
    static uint8_t shape[16];
    (void)ctx, (void)bank, (void)tag;
    return shape;
}
static int32_t mf_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)ctx, (void)x0, (void)x1, (void)y0, (void)y1;
    return 0;
}
static int32_t mf_picture(void *ctx, int32_t p) { (void)ctx; return p; }
static int32_t mf_nothing_int(void *ctx) { (void)ctx; return 0; }
/* the screen arms the keys once the resting ticks pass REST, so the script's first key is taken */
static int32_t mf_ticks(void *ctx) { (void)ctx; return 100; }
static void mf_shape_arg(void *ctx, const uint8_t *s) { (void)ctx, (void)s; }
static void mf_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    (void)ctx, (void)style, (void)text, (void)x, (void)y;
}
static int32_t mf_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static void mf_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)ctx, (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
}
static void mf_save(void *ctx, const PortExtras *e, int16_t unlock)
{
    MenuFake *f = ctx;
    f->saves++;
    f->saved = *e;
    (void)unlock;
}

static void run_menu(PortExtras *e, const int *keys, int count, int combine, MenuFake *f)
{
    memset(f, 0, sizeof *f);
    f->keys = keys, f->key_count = count;
    int16_t unlock = 0;
    const ExtrasMenuOps ops = {
        .pointer_move_to = mf_move, .load_art = mf_art, .shape = mf_shape, .palette_set = mf_six,
        .pointer_in_rect = mf_in_rect, .select = mf_one, .blit_own = mf_shape_arg, .picture_shape = mf_picture,
        .select_default = mf_void, .pointer_hide = mf_void, .flip = mf_void, .pointer_show = mf_void,
        .pointer_buttons = mf_nothing_int, .text_styles = mf_void, .fade_in = mf_four, .deadline_set = mf_one,
        .deadline_passed = mf_nothing_int, .sound = mf_one, .ticks = mf_ticks,
        .pointer_activate_if_moved = mf_void,
        .key = mf_key, .pointer_click = mf_void, .pointer_off = mf_void, .music = mf_void,
        .plus_text = mf_text, .plus_text_width = mf_text_width, .plus_panel = mf_panel,
        .extras_save = mf_save, .ctx = f, .port_combine_variants = combine };
    extras_menu_run(e, &unlock, 1, &ops);
}

/* DOWN `count` times, appended to keys from n on; returns the new count */
static int downs(int *keys, int n, int count)
{
    for (int i = 0; i < count; i++) keys[n++] = DOWN;
    return n;
}

/* the keys from the screen's first row to `row`: on page 1, DOWN to it; on page 2, DOWN to page 1's Done, RIGHT to
 * Cancel, RIGHT round to page 2's first row, then DOWN to `row`. Returns how many keys were written. */
static int keys_to_row(int *keys, int row)
{
    if (row < ROW_FIRST_P2) return downs(keys, 0, row);
    int n = downs(keys, 0, ROWS_P1);
    keys[n++] = RIGHT, keys[n++] = RIGHT;
    return downs(keys, n, row - ROW_FIRST_P2);
}

/* the script both menu tests share: on page 1, Rally on, SPACE, UP to Mirror, SPACE on, then down the page to Done,
 * ENTER */
static int mirror_and_rally_keys(int *keys)
{
    int n = keys_to_row(keys, ROW_RALLY);
    keys[n++] = SPACE, keys[n++] = UP, keys[n++] = SPACE;
    n = downs(keys, n, ROWS_P1 - ROW_MIRROR);   /* from Mirror down to Done */
    keys[n++] = ENTER;
    return n;
}

/* Rally on, then Mirror on: the port leaves both, and Done saves both. */
static void menu_both_stay_on_under_the_port(void)
{
    PortExtras e = port_extras_defaults();
    MenuFake f;
    int keys[48];
    run_menu(&e, keys, mirror_and_rally_keys(keys), 1, &f);
    CHECK_EQ_INT(f.saves, 1);
    CHECK_EQ_INT(f.saved.mirror, 1);
    CHECK_EQ_INT(f.saved.rally, 1);
}

/* The exe excludes the other: the port's switch off keeps that, so the original's own answer is still there. */
static void menu_still_excludes_as_the_exe(void)
{
    PortExtras e = port_extras_defaults();
    MenuFake f;
    int keys[48];
    run_menu(&e, keys, mirror_and_rally_keys(keys), 0, &f);
    CHECK_EQ_INT(f.saves, 1);
    CHECK_EQ_INT(f.saved.mirror, 1);
    CHECK_EQ_INT(f.saved.rally, 0);
}

/* Either one alone with the port's rule on: the other stays out, as before. */
static void menu_alone_is_unchanged_under_the_port(void)
{
    PortExtras e = port_extras_defaults();
    MenuFake f;
    /* Mirror on (page 1), down the page to Done, ENTER — rally untouched */
    int keys[48];
    int n = keys_to_row(keys, ROW_MIRROR);
    keys[n++] = SPACE;
    n = downs(keys, n, ROWS_P1 - ROW_MIRROR);   /* from Mirror down to Done */
    keys[n++] = ENTER;
    run_menu(&e, keys, n, 1, &f);
    CHECK_EQ_INT(f.saves, 1);
    CHECK_EQ_INT(f.saved.mirror, 1);
    CHECK_EQ_INT(f.saved.rally, 0);
}

/* ----- central_draw.c / central_glue.c: the thumbnail and the TR1 -> TR5 remap ----- */

/* Which of the two variants' thumbnail banks the track art takes: the exe gives "m" 10 and "r" 19, and 10 when
 * both are set. The port's rule gives rally (19) when the two are combined. */
static void thumbnail_base_prefers_rally_when_combined(void)
{
    CHECK_EQ_INT(central_track_art_base(1, 0, 0), 10);    /* mirror */
    CHECK_EQ_INT(central_track_art_base(0, 1, 0), 19);    /* rally */
    CHECK_EQ_INT(central_track_art_base(0, 0, 0), 0);     /* neither */
    CHECK_EQ_INT(central_track_art_base(1, 1, 0), 10);    /* the exe: mirror wins */
    CHECK_EQ_INT(central_track_art_base(1, 1, 1), 19);    /* the port: rally */
}

/* The stem a rally race runs on Rusty Springs (track 3): "tr5" in place of "tr1". */
static void rally_on_tr1_runs_tr5(void)
{
    const char *const tr1 = "TR1";
    CHECK(strcmp(rally_track_stem(tr1, 0, 0, 0), tr1) == 0);
    CHECK(strcmp(rally_track_stem(tr1, 1, 0, 0), tr1) == 0);      /* mirror alone: no remap */
    CHECK(strcmp(rally_track_stem(tr1, 0, 1, 0), "TR5") == 0);    /* rally alone */
    CHECK(strcmp(rally_track_stem(tr1, 1, 1, 0), "TR1") == 0);    /* the exe: mirror wins, no remap */
    CHECK(strcmp(rally_track_stem(tr1, 1, 1, 1), "TR5") == 0);    /* the port: rally's own track */
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    /* ----- track_fam.c ----- */
    fam_mirror_wins_as_the_exe();
    fam_rally_wins_when_the_port_says_so();
    fam_alone_is_the_exe_with_the_flag_on();
    fam_other_time_keeps_the_t_art();
    fam_track_three_has_no_rally_art();
    /* ----- extras_apply.c ----- */
    reapply_both_is_the_exe();
    reapply_both_keeps_the_two_when_the_port_says_so();
    reapply_alone_is_unchanged_by_the_port();
    reapply_alone_keeps_the_picked_variant();
    reapply_none_touches_nothing();
    from_flags_both_shows_both_under_the_port();
    /* ----- extras_menu.c ----- */
    menu_both_stay_on_under_the_port();
    menu_still_excludes_as_the_exe();
    menu_alone_is_unchanged_under_the_port();
    /* ----- central_draw.c / central_glue.c ----- */
    thumbnail_base_prefers_rally_when_combined();
    rally_on_tr1_runs_tr5();
    return TEST_REPORT();
}
