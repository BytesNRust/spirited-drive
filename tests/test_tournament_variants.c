/* dep56, Restomod's tournament variants (tournament_variants.h): chosen on the switches before the first race, fixed
 * for the whole run once a race is run, and kept with a save in its own file (NAME.VAR beside NAME.SAV), written at
 * every save and read at every load; a save without one gives the base and a log line, never the variants of the
 * tournament that was in memory. The races read them through race_settings, the Lunar Springs rule and the standings
 * line as any race does. */
#include "game/frontend/records_route.h"
#include "game/frontend/tournament_variants.h"
#include "game/platform/platform.h"
#include "game/race/lunar_springs.h"
#include "game/sys/port_log.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

/* the player's directory: a few files in memory */
enum { FILES = 6, BYTES = 256 };
typedef struct {
    char name[32];
    uint8_t bytes[BYTES];
    size_t size;
    int present;
} File;
static File files[FILES];

static File *find(const char *name)
{
    for (int i = 0; i < FILES; i++)
        if (files[i].present && strcmp(files[i].name, name) == 0) return &files[i];
    return NULL;
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    const File *f = find(name);
    if (f == NULL) return NULL;
    uint8_t *copy = malloc(f->size ? f->size : 1);
    memcpy(copy, f->bytes, f->size);
    *size = f->size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    File *f = find(name);
    for (int i = 0; f == NULL && i < FILES; i++)
        if (!files[i].present) f = &files[i];
    if (f == NULL || size > BYTES) return false;
    snprintf(f->name, sizeof f->name, "%s", name);
    memcpy(f->bytes, data, size);
    f->size = size, f->present = 1;
    return true;
}

bool platform_remove_user_file(const char *name)
{
    File *f = find(name);
    if (f == NULL) return false;
    f->present = 0;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

bool platform_user_file_exists(const char *name) { return find(name) != NULL; }   /* race_records.c's, linked with the standings line */

static void put_text(const char *name, const char *text) { platform_write_user_file(name, (const uint8_t *)text, strlen(text)); }

static void clear_files(void) { memset(files, 0, sizeof files); }

static int same(TournamentVariants a, TournamentVariants b)
{
    return a.mirror == b.mirror && a.inverse == b.inverse && a.rally == b.rally;
}

static void test_the_path_beside_the_save(void)
{
    char out[TOURNAMENT_VARIANTS_PATH];
    CHECK(tournament_variants_path("EDGAR.SAV", out, sizeof out));
    CHECK(strcmp(out, "EDGAR.VAR") == 0);
    CHECK(tournament_variants_path("mine.sav", out, sizeof out));
    CHECK(strcmp(out, "mine.VAR") == 0);
    CHECK(!tournament_variants_path("REPLAY.RPL", out, sizeof out));
    CHECK(!tournament_variants_path("SAV", out, sizeof out));
    CHECK(!tournament_variants_path("LONGNAME.SAV", out, 8));   /* no room: nothing written past it */
}

static void test_the_text_and_its_reading(void)
{
    char text[96];
    const TournamentVariants v = { 1, 0, 1 };
    const int n = tournament_variants_format(v, text, sizeof text);
    CHECK(strcmp(text, "tnfs-tournament-variants 1\nmirror=1\ninverse=0\nrally=1\n") == 0);
    TournamentVariants back = { 0, 1, 0 };
    CHECK(tournament_variants_parse((const uint8_t *)text, (size_t)n, &back));
    CHECK(same(back, v));
    static const char *const BAD[] = {
        "", "mirror=1\ninverse=0\nrally=1\n", "tnfs-tournament-variants 2\nmirror=1\ninverse=0\nrally=1\n",
        "tnfs-tournament-variants 1\nmirror=1\ninverse=0\n", "tnfs-tournament-variants 1\nmirror=2\ninverse=0\nrally=1\n",
        "tnfs-tournament-variants 1\nmirror=1\ninverse=0\nrally=1\nweather=3\n", "tnfs-tournament-variants 1\nmirror=11\ninverse=0\nrally=1\n",
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        TournamentVariants kept = { 1, 1, 1 };
        CHECK(!tournament_variants_parse((const uint8_t *)BAD[i], strlen(BAD[i]), &kept));
        CHECK(kept.mirror == 1 && kept.inverse == 1 && kept.rally == 1);   /* untouched */
    }
    const char crlf[] = "tnfs-tournament-variants 1\r\nmirror=0\r\ninverse=1\r\nrally=0\r\n";
    CHECK(tournament_variants_parse((const uint8_t *)crlf, strlen(crlf), &back));
    CHECK(back.mirror == 0 && back.inverse == 1 && back.rally == 0);
    const uint8_t nul[] = "tnfs-tournament-variants 1\0mirror=1";
    CHECK(!tournament_variants_parse(nul, sizeof nul - 1, &back));
}

static void test_fixed_once_a_race_is_run(void)
{
    int16_t races[TOURNAMENT_VARIANTS_TRACKS] = { 0 };
    CHECK(!tournament_variants_locked(races));
    races[5] = 1;
    CHECK(tournament_variants_locked(races));
}

/* the switches the track screen showed take their value; the ones it did not keep the tournament's */
static void test_the_switches_followed_where_shown(void)
{
    const TournamentVariants was = { 1, 1, 1 };
    CHECK(same(tournament_variants_follow(was, 3, 0, 0, 0), (TournamentVariants){ 0, 0, 0 }));
    CHECK(same(tournament_variants_follow(was, 1, 0, 0, 0), (TournamentVariants){ 0, 1, 0 }));   /* a stage: no Inverse row */
    CHECK(same(tournament_variants_follow(was, 8, 0, 0, 0), (TournamentVariants){ 1, 0, 1 }));   /* Lost Vegas: Inverse alone */
    CHECK(same(tournament_variants_follow(was, 9, 0, 0, 0), was));                                /* the hidden track: none */
}

/* the whole run: every race on every track runs the variants chosen, Lost Vegas without mirror and rally; Rusty Springs'
 * rally follows the Lunar Springs Track switch; the standings line names the layout */
static void test_every_race_of_the_run(void)
{
    tournament_variants_reset();
    tournament_variants_set((TournamentVariants){ 1, 1, 1 });
    int16_t races[TOURNAMENT_VARIANTS_TRACKS] = { 0 };
    for (int track = 0; track < 8; track++) {
        const TournamentVariants v = tournament_variants_on_track(tournament_variants_get(), track);
        CHECK(same(v, (TournamentVariants){ 1, 1, 1 }));
        races[track] = 1;
        CHECK(tournament_variants_locked(races));
        char line[64];
        records_table_line(records_race_kind(v.mirror, v.rally, track >= 3 && v.inverse, 0, 0, 0).layout, 0, 0, line, sizeof line);
        CHECK(strstr(line, "MIRROR") != NULL && strstr(line, "RALLY") != NULL);
        if (track >= 3) CHECK(strstr(line, "REVERSE") != NULL);
    }
    CHECK(same(tournament_variants_on_track(tournament_variants_get(), 8), (TournamentVariants){ 0, 1, 0 }));
    CHECK(same(tournament_variants_on_track(tournament_variants_get(), 9), (TournamentVariants){ 0, 0, 0 }));
    const TournamentVariants rusty = tournament_variants_on_track(tournament_variants_get(), 3);
    CHECK(lunar_springs_active(3, rusty.rally, 1));    /* the Lunar Springs Track switch on: the lunar track */
    CHECK(!lunar_springs_active(3, rusty.rally, 0));   /* off: the desert */
    tournament_variants_set((TournamentVariants){ 0, 0, 0 });
    char line[64];
    const TournamentVariants base = tournament_variants_on_track(tournament_variants_get(), 3);
    records_table_line(records_race_kind(base.mirror, base.rally, base.inverse, 0, 0, 0).layout, 0, 0, line, sizeof line);
    CHECK(strncmp(line, "NORMAL", 6) == 0);
}

/* the owner's: a tournament not on the base variants, race 1, saved; another tournament saved in a second slot and
 * loaded; the first loaded again: its own variants, still fixed, on the races after and in the standings */
static void test_saved_and_loaded_again(void)
{
    clear_files();
    tournament_variants_reset();
    int16_t races[TOURNAMENT_VARIANTS_TRACKS] = { 0 };
    tournament_variants_set((TournamentVariants){ 1, 0, 1 });
    races[0] = 1;   /* race 1 run */
    CHECK(tournament_variants_locked(races));
    CHECK(tournament_variants_save("FIRST.SAV"));
    CHECK(find("FIRST.VAR") != NULL);

    tournament_variants_reset();   /* another tournament, in another slot */
    tournament_variants_set((TournamentVariants){ 0, 1, 0 });
    CHECK(tournament_variants_save("OTHER.SAV"));
    tournament_variants_load("OTHER.SAV");
    CHECK(same(tournament_variants_get(), (TournamentVariants){ 0, 1, 0 }));

    tournament_variants_load("FIRST.SAV");
    CHECK(same(tournament_variants_get(), (TournamentVariants){ 1, 0, 1 }));
    CHECK(tournament_variants_locked(races));   /* the races come with the .SAV: still fixed */
    for (int track = 1; track < 8; track++) {
        const TournamentVariants v = tournament_variants_on_track(tournament_variants_get(), track);
        CHECK(v.mirror == 1 && v.rally == 1 && v.inverse == 0);
        char line[64];
        records_table_line(records_race_kind(v.mirror, v.rally, v.inverse, 0, 0, 0).layout, 0, 0, line, sizeof line);
        CHECK(strncmp(line, "MIRROR + RALLY", 14) == 0);
    }
    /* each slot has its own: saving the first again leaves the other's as it was */
    CHECK(tournament_variants_save("FIRST.SAV"));
    tournament_variants_load("OTHER.SAV");
    CHECK(same(tournament_variants_get(), (TournamentVariants){ 0, 1, 0 }));
}

/* a save from before this change (no .VAR), or a damaged one: the base and a log line, not the tournament in memory */
static void test_no_file_is_the_base(void)
{
    clear_files();
    tournament_variants_set((TournamentVariants){ 1, 1, 1 });
    port_log_watch("OLD.VAR missing, base variants");
    tournament_variants_load("OLD.SAV");
    CHECK(same(tournament_variants_get(), (TournamentVariants){ 0, 0, 0 }));
    CHECK(port_log_watch_hit());
    tournament_variants_set((TournamentVariants){ 1, 1, 1 });
    put_text("BAD.VAR", "tnfs-tournament-variants 1\nmirror=yes\n");
    port_log_watch("BAD.VAR damaged, base variants");
    tournament_variants_load("BAD.SAV");
    CHECK(same(tournament_variants_get(), (TournamentVariants){ 0, 0, 0 }));
    CHECK(port_log_watch_hit());
}

/* deleted with its save; a save without one deletes quietly */
static void test_removed_with_its_save(void)
{
    clear_files();
    tournament_variants_set((TournamentVariants){ 1, 0, 0 });
    CHECK(tournament_variants_save("GONE.SAV"));
    CHECK(find("GONE.VAR") != NULL);
    tournament_variants_remove("GONE.SAV");
    CHECK(find("GONE.VAR") == NULL);
    port_log_watch("cannot remove");
    tournament_variants_remove("GONE.SAV");
    tournament_variants_remove("REPLAY.RPL");
    CHECK(!port_log_watch_hit());
}

/* the flag rule: a mode changed with a Restomod tournament loaded. Stock and Restored run the exe's tournament (none on);
 * back in Restomod the tournament's are on again. A Stock tournament that never had any is left as it is. */
static void test_a_mode_change_mid_run(void)
{
    const TournamentVariants chosen = { 1, 1, 1 };
    TournamentVariants out = { 9, 9, 9 };
    CHECK(tournament_variants_settings(chosen, 3, 1, 1, 1, 2, &out));
    CHECK(same(out, chosen));
    CHECK(tournament_variants_settings(chosen, 3, 0, 1, 1, 2, &out));   /* Stock or Restored now */
    CHECK(same(out, (TournamentVariants){ 0, 0, 0 }));
    CHECK(tournament_variants_settings(chosen, 3, 1, 1, 1, 2, &out));   /* Restomod again */
    CHECK(same(out, chosen));
    out = (TournamentVariants){ 9, 9, 9 };
    CHECK(!tournament_variants_settings((TournamentVariants){ 0, 0, 0 }, 3, 0, 1, 1, 2, &out));   /* Stock's own: untouched */
    CHECK(out.mirror == 9);
    CHECK(tournament_variants_settings(chosen, 3, 1, 1, 0, 1, &out));   /* level 1: the mirror alone */
    CHECK(same(out, (TournamentVariants){ 1, 0, 0 }));
    CHECK(tournament_variants_settings(chosen, 8, 1, 1, 1, 2, &out));   /* Lost Vegas */
    CHECK(same(out, (TournamentVariants){ 0, 1, 0 }));
}

int main(void)
{
    test_a_mode_change_mid_run();
    test_the_path_beside_the_save();
    test_the_text_and_its_reading();
    test_fixed_once_a_race_is_run();
    test_the_switches_followed_where_shown();
    test_every_race_of_the_run();
    test_saved_and_loaded_again();
    test_no_file_is_the_base();
    test_removed_with_its_save();
    return TEST_REPORT();
}
