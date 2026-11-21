/* Opening a car project (editor/car_project.c): a kit in the current layout (car_kit_paths.h) opens from its folder; the car file is the build's, so
 * a kit not built yet is still a kit; the old coche/ + modelo/ layout is refused in words; what is not a car says what it looked at and what is
 * missing; the lists are the files found, by name. Fixtures are made here. */
#include "car_project.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[512];

static void path_in(char *out, size_t cap, const char *a, const char *b)
{
    snprintf(out, cap, "%s/%s%s%s", base, a, b[0] != '\0' ? "/" : "", b);
}

static void make_dir(const char *rel)
{
    char p[1024];
    path_in(p, sizeof p, rel, "");
    mkdir(p, 0777);
}

static void system_rm(const char *rel)
{
    char p[1024], cmd[1100];
    path_in(p, sizeof p, rel, "");
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", p);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", p);
}

static void write_file(const char *rel, const char *text)
{
    char p[1024];
    path_in(p, sizeof p, rel, "");
    FILE *f = fopen(p, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

static const char CARD[] = "id=carrera-84\nname=911 Carrera 3.2\nversion=0.2\ntype=car\nstem=C84\nshort_name=911 3.2\nyear=1984\npower=231 hp\n"
                           "weight=1160 kg\npaints=Guards Red:c8102e,Black:141414\nauthor=Edgar\n";

/* kit/ in the current layout (car_kit_paths.h): mod.ini, model/near/bank and model/far/bank (PNG files), car_masks (flat PNG files), and
 * build/piece/SIMDATA/CARFAMS/C84.CFM when `built` */
static void make_kit(const char *kit, int built)
{
    char p[256];
    static const char *const DIRS[] = { "model", "model/near", "model/near/bank", "model/far", "model/far/bank", "car_masks", "build", "build/piece",
                                        "build/piece/SIMDATA", "build/piece/SIMDATA/CARFAMS" };
    make_dir(kit);
    for (size_t i = 0; i < sizeof DIRS / sizeof DIRS[0]; i++) {
        if (!built && strncmp(DIRS[i], "build", 5) == 0) continue;
        snprintf(p, sizeof p, "%s/%s", kit, DIRS[i]);
        make_dir(p);
    }
    snprintf(p, sizeof p, "%s/mod.ini", kit), write_file(p, CARD);
    if (built) snprintf(p, sizeof p, "%s/build/piece/SIMDATA/CARFAMS/C84.CFM", kit), write_file(p, "cfm");
    static const char *const MASKS[] = { "rsid_mask.png", "frnt_mask.png", "bott_mask.png", "notes.txt" };
    for (size_t i = 0; i < sizeof MASKS / sizeof MASKS[0]; i++) {
        snprintf(p, sizeof p, "%s/car_masks/%s", kit, MASKS[i]), write_file(p, "x");
    }
    static const char *const TEX[] = { "0010_rsid.png", "0001_topv.png", "0000__PAL.bin", "entries.tsv", "0003_bott.png" };
    for (size_t i = 0; i < sizeof TEX / sizeof TEX[0]; i++) {
        snprintf(p, sizeof p, "%s/model/near/bank/%s", kit, TEX[i]), write_file(p, "x");
    }
    snprintf(p, sizeof p, "%s/model/far/bank/0001_frnt.png", kit), write_file(p, "x");
    snprintf(p, sizeof p, "%s/model/far/bank/0002_side.png", kit), write_file(p, "x");
}

static int detect(const char *rel, CarProject *c, char *why)
{
    char dir[1024];
    path_in(dir, sizeof dir, rel, "");
    why[0] = '\0';
    return car_project_detect(dir, c, why, 400);
}

static void check_kit(void)
{
    static CarProject c;
    char why[400], want[1024];
    CHECK_EQ_INT(detect("kit", &c, why), 1);
    path_in(want, sizeof want, "kit", "");
    CHECK(strcmp(c.root, want) == 0 && strstr(c.piece, "kit/build/piece") != NULL);   /* mod.ini is at the kit's top; the game installs build/piece */
    CHECK(strcmp(c.kit, "kit") == 0 && !c.masks_only);
    CHECK(strcmp(c.info.id, "carrera-84") == 0 && strcmp(c.info.stem, "C84") == 0 && c.info.type == MOD_TYPE_CAR);
    CHECK(strcmp(c.info.name, "911 Carrera 3.2") == 0 && c.info.paint_count == 2);
    CHECK(c.built && strstr(c.cfm, "kit/build/piece/SIMDATA/CARFAMS/C84.CFM") != NULL);
    CHECK(c.has_masks_dir && c.mask_count == 3);   /* the PNG files only, by name, no stem level */
    CHECK(strcmp(c.mask[0], "bott_mask.png") == 0 && strcmp(c.mask[1], "frnt_mask.png") == 0 && strcmp(c.mask[2], "rsid_mask.png") == 0);
    CHECK(c.has_model && c.texture_count == 3 && c.far_count == 2);
    CHECK(strcmp(c.texture[0], "0001_topv.png") == 0 && strcmp(c.texture[2], "0010_rsid.png") == 0);
    char dir[1024];
    CHECK_EQ_INT(detect("kit/", &c, why), 1);   /* a trailing slash */
    path_in(dir, sizeof dir, "kit/../kit", "");   /* a path that goes round */
    CHECK_EQ_INT(car_project_detect(dir, &c, why, sizeof why), 1);
    CHECK_EQ_INT(car_project_present(dir), 1);
    CHECK(strcmp(c.kit, "kit") == 0);
    car_project_masks_dir(&c, dir, sizeof dir);
    path_in(want, sizeof want, "kit/car_masks", "");
    CHECK(strcmp(dir, want) == 0);
    car_project_card_dir(&c, dir, sizeof dir);   /* the card's own files (mod.ini, card.png) are the kit's, not build/piece's (disposable) */
    path_in(want, sizeof want, "kit", "");
    CHECK(strcmp(dir, want) == 0);
}

/* what Export original car makes of a car you drive (and a `car new` piece): mod.ini and SIMDATA at the top, masks under car_masks/<folder>/. It opens as it always did */
static void check_exported_original_shape(void)
{
    static CarProject c;
    char why[400], dir[1024], want[1024];
    make_dir("orig");
    make_dir("orig/SIMDATA");
    make_dir("orig/SIMDATA/CARFAMS");
    make_dir("orig/car_masks");
    make_dir("orig/car_masks/C84");
    write_file("orig/mod.ini", CARD);
    write_file("orig/SIMDATA/CARFAMS/C84.CFM", "cfm");
    write_file("orig/car_masks/C84/rsid_mask.png", "x");
    write_file("orig/car_masks/C84/frnt_mask.png", "x");
    CHECK_EQ_INT(detect("orig", &c, why), 1);
    CHECK(c.legacy && !c.masks_only && c.built && !c.has_model);
    path_in(want, sizeof want, "orig", "");
    CHECK(strcmp(c.root, want) == 0 && strcmp(c.piece, want) == 0);   /* nothing is built: the piece is the folder itself */
    car_project_card_dir(&c, dir, sizeof dir);
    CHECK(strcmp(dir, want) == 0);   /* and it holds the card's files */
    CHECK(strstr(c.cfm, "orig/SIMDATA/CARFAMS/C84.CFM") != NULL && strcmp(c.folder, "C84") == 0);
    CHECK(c.has_masks_dir && c.mask_count == 2);
    car_project_masks_dir(&c, dir, sizeof dir);
    path_in(want, sizeof want, "orig/car_masks/C84", "");
    CHECK(strcmp(dir, want) == 0);   /* the stem level, as the game's data/car_masks/<folder>/ */
    path_in(dir, sizeof dir, "orig", "");
    CHECK_EQ_INT(car_project_present(dir), 1);
    make_dir("lone");   /* a piece with no masks */
    make_dir("lone/SIMDATA");
    make_dir("lone/SIMDATA/CARFAMS");
    write_file("lone/mod.ini", CARD);
    write_file("lone/SIMDATA/CARFAMS/C84.CFM", "cfm");
    CHECK_EQ_INT(detect("lone", &c, why), 1);
    CHECK(c.legacy && !c.has_masks_dir && c.mask_count == 0);
    make_dir("nocfm");   /* a card with no car file and no model/: said in the game's words */
    write_file("nocfm/mod.ini", CARD);
    CHECK_EQ_INT(detect("nocfm", &c, why), 0);
    CHECK(strstr(why, "C84.CFM") != NULL && strstr(why, "SIMDATA/CARFAMS") != NULL);
}

static void check_not_built_yet(void)
{
    static CarProject c;
    char why[400];
    make_kit("fresh", 0);
    CHECK_EQ_INT(detect("fresh", &c, why), 1);   /* the car file is made by the build: a kit with none yet is a kit */
    CHECK(!c.built && strstr(c.cfm, "fresh/build/piece/SIMDATA/CARFAMS/C84.CFM") != NULL);
}

static void check_old_layout_is_refused(void)
{
    static CarProject c;
    char why[400];
    make_dir("old");   /* coche/ + modelo/, as the first kits were */
    make_dir("old/coche");
    make_dir("old/modelo");
    make_dir("old/coche/SIMDATA");
    make_dir("old/coche/SIMDATA/CARFAMS");
    write_file("old/coche/mod.ini", CARD);
    write_file("old/coche/SIMDATA/CARFAMS/C84.CFM", "cfm");
    char dir[1024];
    CHECK_EQ_INT(detect("old", &c, why), 0);
    CHECK(strstr(why, "not a kit in the current layout") != NULL && strstr(why, "old") != NULL);
    path_in(dir, sizeof dir, "old", "");
    CHECK_EQ_INT(car_project_present(dir), 1);   /* it looks like a car: opening it says why it is refused, it is not made a track */
    CHECK_EQ_INT(detect("old/coche", &c, why), 0);   /* the old kit's coche/ folder, with its modelo/ beside it: refused, though it looks like a piece */
    CHECK(strstr(why, "not a kit in the current layout") != NULL);
    CHECK_EQ_INT(c.info.id[0], 0);
}

static void check_what_is_not_a_car(void)
{
    static CarProject c;
    char why[400];
    make_dir("empty");
    CHECK_EQ_INT(detect("empty", &c, why), 0);
    CHECK(strstr(why, "empty") != NULL && strstr(why, "mod.ini") != NULL);   /* what it looked at */
    CHECK_EQ_INT(c.info.id[0], 0);
    char dir[1024];
    path_in(dir, sizeof dir, "empty", "");
    CHECK_EQ_INT(car_project_present(dir), 0);
    CHECK_EQ_INT(detect("nowhere", &c, why), 0);
    CHECK(strstr(why, "not a folder") != NULL);
    static const struct { const char *dir, *card, *want; } BAD[] = {
        { "track", "id=t\nname=T\nversion=1\ntype=track\nstem=TT\n", "type=track" },   /* a mod.ini that is a track */
        { "tweak", "id=t\nname=T\nversion=1\n", "type=car" },                          /* no type: a tweak */
        { "broken", "name=A\nversion=1\ntype=car\n", "no id" },                        /* a card the game would refuse: the game's own words */
        { "nostem", "id=a\nname=A\nversion=1\ntype=car\n", "stem=" },
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        char sub[64];
        make_dir(BAD[i].dir);
        snprintf(sub, sizeof sub, "%s/model", BAD[i].dir), make_dir(sub);
        snprintf(sub, sizeof sub, "%s/mod.ini", BAD[i].dir), write_file(sub, BAD[i].card);
        CHECK_EQ_INT(detect(BAD[i].dir, &c, why), 0);
        CHECK(strstr(why, BAD[i].want) != NULL);
        for (const char *p = why; *p != '\0'; p++) CHECK((unsigned char)*p >= 0x20 || *p == '\n');
    }
    CHECK_EQ_INT(detect("kit/model", &c, why), 0);   /* a folder of the kit that is not the car */
    CHECK_EQ_INT(detect("kit/car_masks", &c, why), 0);
    CHECK_EQ_INT(car_project_detect(NULL, &c, why, sizeof why), 0);
}

static void check_no_masks_is_a_car(void)
{
    static CarProject c;
    char why[400];
    make_kit("bare", 1);
    system_rm("bare/car_masks");
    CHECK_EQ_INT(detect("bare", &c, why), 1);   /* no masks yet is listed as none, not refused */
    CHECK(!c.has_masks_dir && c.mask_count == 0);
}

static void check_long_lists(void)
{
    static CarProject c;
    char why[400], p[256];
    make_kit("big", 1);
    for (int i = 0; i < CAR_PROJECT_LIST + 6; i++) {
        snprintf(p, sizeof p, "big/model/near/bank/%04d_t%02d.png", i, i);
        write_file(p, "x");
    }
    CHECK_EQ_INT(detect("big", &c, why), 1);
    CHECK_EQ_INT(c.texture_count, 3 + CAR_PROJECT_LIST + 6);   /* the count is all of them, the names the first ones */
    CHECK(strcmp(c.texture[0], "0000_t00.png") == 0);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(base, sizeof base, "%s/tnfs-car-project-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(base) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    char real[512];
    if (realpath(base, real) != NULL) snprintf(base, sizeof base, "%s", real);   /* the name detect gives: a symlinked TMPDIR is another name */
    make_kit("kit", 1);
    check_kit();
    check_exported_original_shape();
    check_not_built_yet();
    check_old_layout_is_refused();
    check_what_is_not_a_car();
    check_no_masks_is_a_car();
    check_long_lists();
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
