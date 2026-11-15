/* dep59 step 1: the added cars come from the mod folders in effect (game/sys/car_pieces.h). A folder whose mod.ini says type=car and
 * whose files pass car_piece_check is a car, in Restomod only; one that does not is refused with one log line, and a folder of another
 * type, or with no mod.ini, is passed over. A reload keeps the numbers of the cars that stay; car_pieces_follow loads again only when the
 * mods in effect changed (mod_set_generation). The pieces are copies of the CD's Viper (skipped without the CD, as test_models). */
#include "game/race/car_registry.h"
#include "game/sys/car_pieces.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_profile.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256], cd[300], pref[300];

static void make_dirs(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static int copy(const char *from, const char *dir, const char *name)
{
    FILE *in = fopen(from, "rb");
    if (in == NULL) return 0;
    char path[1200];
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *out = fopen(path, "wb");
    char buf[4096];
    size_t n;
    while (out != NULL && (n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    if (out != NULL) fclose(out);
    return out != NULL;
}

static void write_text(const char *dir, const char *name, const char *text)
{
    char path[1200];
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

/* a piece folder <base>/<folder> of id `id` and stem `stem`: its card (`extra` lines after it) and the Viper's files, those of `leave`
 * ("CFM", "PDN", "PBS" or "") left out; its path in `out` */
static int piece(const char *folder, const char *id, const char *stem, const char *extra, const char *leave, char out[400])
{
    char dir[400], card[600], src[600], name[64];
    snprintf(out, 400, "%s/%s", base, folder);
    snprintf(card, sizeof card, "id=%s\nname=%s\nversion=1\ntype=car\nstem=%s\nshort_name=T\n%s", id, id, stem, extra);
    write_text(out, "mod.ini", card);
    static const char *const KIND[3] = { "CFM", "PDN", "PBS" };
    for (int i = 0; i < 3; i++) {
        if (strcmp(leave, KIND[i]) == 0) continue;
        snprintf(src, sizeof src, "%s/original/SIMDATA/%s/DVIPER.%s", TNFS_SOURCE_DIR, i == 2 ? "CARSPECS" : "CARFAMS", KIND[i]);
        snprintf(dir, sizeof dir, "%s/SIMDATA/%s", out, i == 2 ? "CARSPECS" : "CARFAMS");
        snprintf(name, sizeof name, "%s.%s", stem, KIND[i]);
        if (!copy(src, dir, name)) return 0;
    }
    return 1;
}

static const char FROM[] = "sound_from=viper\nai_from=viper\n";

static int load1(const char *dir, int restomod)
{
    const char *dirs[1] = { dir };
    return car_pieces_load(dirs, 1, restomod);
}

static void test_one_piece(void)
{
    char a[400];
    CHECK(piece("a", "test-car", "TESTCAR", FROM, "", a));
    car_registry_clear();
    CHECK_EQ_INT(load1(a, 1), 1);
    CHECK_EQ_INT(car_registry_find("test-car"), 16);
    CHECK(strcmp(car_registry_stem(16), "TESTCAR") == 0);
    CHECK_EQ_INT(car_registry_sound_model(16), 5);
    CHECK_EQ_INT(car_registry_ai_model(16), 5);
    CHECK_EQ_INT(load1(a, 1), 1);   /* again: the same */
    CHECK_EQ_INT(car_registry_find("test-car"), 16);
    CHECK_EQ_INT(load1(a, 0), 0);   /* not Restomod: none */
    CHECK_EQ_INT(car_registry_count(), 0);
}

static void test_refused_and_passed_over(void)
{
    char d[400];
    static const struct { const char *folder, *extra, *leave; } BAD[] = {
        { "no_pdn", "sound_from=viper\nai_from=viper\n", "PDN" }, { "no_pbs", "sound_from=viper\nai_from=viper\n", "PBS" },
        { "no_cfm", "sound_from=viper\nai_from=viper\n", "CFM" }, { "lada", "sound_from=lada\nai_from=viper\n", "" },
        { "no_ai", "sound_from=viper\n", "" },
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        CHECK(piece(BAD[i].folder, "bad", "BADCAR", BAD[i].extra, BAD[i].leave, d));
        car_registry_clear();
        CHECK_EQ_INT(load1(d, 1), 0);
        CHECK_EQ_INT(car_registry_count(), 0);
    }
    snprintf(d, sizeof d, "%s/no_stem", base);   /* no stem */
    write_text(d, "mod.ini", "id=x\nname=X\nversion=1\ntype=car\nsound_from=viper\nai_from=viper\n");
    CHECK_EQ_INT(load1(d, 1), 0);
    snprintf(d, sizeof d, "%s/tweak", base);      /* another type: not a car, no refusal */
    write_text(d, "mod.ini", "id=t\nname=T\nversion=1\n");
    CHECK_EQ_INT(load1(d, 1), 0);
    snprintf(d, sizeof d, "%s/no_card", base);    /* no mod.ini */
    make_dirs(d);
    CHECK_EQ_INT(load1(d, 1), 0);
    CHECK_EQ_INT(load1(NULL, 1), 0);
}

static void test_numbers_stay_across_reloads(void)
{
    char a[400], b[400], c[400], twin[400];
    CHECK(piece("ra", "car-a", "CARA", FROM, "", a));
    CHECK(piece("rb", "car-b", "CARB", FROM, "", b));
    CHECK(piece("rc", "car-c", "CARC", FROM, "", c));
    CHECK(piece("rtwin", "car-a", "CARTWIN", FROM, "", twin));
    car_registry_clear();
    const char *both[2] = { a, b };
    CHECK_EQ_INT(car_pieces_load(both, 2, 1), 2);
    CHECK_EQ_INT(car_registry_find("car-a"), 16);
    CHECK_EQ_INT(car_registry_find("car-b"), 17);
    CHECK_EQ_INT(car_pieces_load(both, 2, 1), 2);   /* the same folders: the same numbers */
    CHECK_EQ_INT(car_registry_find("car-b"), 17);
    const char *second[1] = { b };
    CHECK_EQ_INT(car_pieces_load(second, 1, 1), 1);   /* the first gone: the second keeps its number */
    CHECK_EQ_INT(car_registry_find("car-a"), -1);
    CHECK_EQ_INT(car_registry_find("car-b"), 17);
    const char *more[2] = { b, c };
    CHECK_EQ_INT(car_pieces_load(more, 2, 1), 2);   /* a new one takes the free number */
    CHECK_EQ_INT(car_registry_find("car-c"), 16);
    const char *twins[2] = { a, twin };               /* two folders of one id: the first wins */
    CHECK_EQ_INT(car_pieces_load(twins, 2, 1), 1);
    CHECK(strcmp(car_registry_stem(car_registry_find("car-a")), "CARA") == 0);
}

static void test_it_follows_the_mods(void)
{
    char a[400];
    CHECK(piece("fa", "follow-car", "FOLLOW", FROM, "", a));
    car_registry_clear();
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(mod_set_start(cd, a));   /* --mods: the piece's folder */
    car_pieces_follow();
    CHECK_EQ_INT(car_registry_find("follow-car"), 16);
    car_registry_clear();
    car_pieces_follow();   /* the mods did not change: nothing is loaded again */
    CHECK_EQ_INT(car_registry_find("follow-car"), -1);
    CHECK(port_profile_select(PROFILE_STOCK));   /* Stock: no mods, no added car */
    car_registry_clear();
    CHECK(car_registry_add(&(CarPiece){ .id = "stale", .stem = "STALE", .sound_model = 0, .ai_model = 0, .dash_model = -1 }, NULL, 0) == 16);
    car_pieces_follow();
    CHECK_EQ_INT(car_registry_count(), 0);
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    car_pieces_follow();
    CHECK_EQ_INT(car_registry_find("follow-car"), 16);
    mod_set_stop();
}

int main(void)
{
    char probe[600];
    snprintf(probe, sizeof probe, "%s/original/SIMDATA/CARSPECS/DVIPER.PBS", TNFS_SOURCE_DIR);
    if (access(probe, R_OK) != 0) {
        fprintf(stderr, "car_pieces: no CD in original/, skipped\n");
        return TEST_REPORT();
    }
    snprintf(base, sizeof base, "%s/tnfs_car_pieces_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(cd, sizeof cd, "%s/cd", base);
    snprintf(pref, sizeof pref, "%s/pref", base);
    make_dirs(cd);
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_one_piece();
    test_refused_and_passed_over();
    test_numbers_stay_across_reloads();
    test_it_follows_the_mods();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
