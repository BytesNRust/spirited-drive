/* Opening a car project (editor/car_project.c): the kit's root and its coche/ folder both open the kit; what is not a car says what it looked at and what is
 * missing; the lists are the files found, by name. Fixtures are made here: a kit with a mod.ini, the car file, masks and textures. */
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

static void write_file(const char *rel, const char *text)
{
    char p[1024];
    path_in(p, sizeof p, rel, "");
    FILE *f = fopen(p, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

static const char CARD[] = "id=carrera-84\nname=911 Carrera 3.2\nversion=0.2\ntype=car\nstem=C84\nshort_name=911 3.2\nyear=1984\npower=231 hp\n"
                           "weight=1160 kg\npaints=Guards Red:c8102e,Black:141414\nauthor=Edgar\n";

/* kit/coche/ with mod.ini, SIMDATA/CARFAMS/C84.CFM and car_masks/C84 (PNG files); kit/modelo/c1 and c3 (PNG files); kit/mascaras (PNG files) */
static void make_kit(const char *kit)
{
    char p[256];
    static const char *const DIRS[] = { "coche", "coche/SIMDATA", "coche/SIMDATA/CARFAMS", "coche/car_masks", "coche/car_masks/C84", "modelo", "modelo/c1",
                                        "modelo/c3", "mascaras" };
    make_dir(kit);
    for (size_t i = 0; i < sizeof DIRS / sizeof DIRS[0]; i++) {
        snprintf(p, sizeof p, "%s/%s", kit, DIRS[i]);
        make_dir(p);
    }
    snprintf(p, sizeof p, "%s/coche/mod.ini", kit), write_file(p, CARD);
    snprintf(p, sizeof p, "%s/coche/SIMDATA/CARFAMS/C84.CFM", kit), write_file(p, "cfm");
    static const char *const MASKS[] = { "rsid_mask.png", "frnt_mask.png", "bott_mask.png", "notes.txt" };
    for (size_t i = 0; i < sizeof MASKS / sizeof MASKS[0]; i++) {
        snprintf(p, sizeof p, "%s/coche/car_masks/C84/%s", kit, MASKS[i]), write_file(p, "x");
    }
    static const char *const TEX[] = { "0010_rsid.png", "0001_topv.png", "0000__PAL.bin", "entries.tsv", "0003_bott.png" };
    for (size_t i = 0; i < sizeof TEX / sizeof TEX[0]; i++) {
        snprintf(p, sizeof p, "%s/modelo/c1/%s", kit, TEX[i]), write_file(p, "x");
    }
    snprintf(p, sizeof p, "%s/modelo/c3/0001_frnt.png", kit), write_file(p, "x");
    snprintf(p, sizeof p, "%s/modelo/c3/0002_side.png", kit), write_file(p, "x");
    snprintf(p, sizeof p, "%s/mascaras/tras.png", kit), write_file(p, "x");
    snprintf(p, sizeof p, "%s/mascaras/frnt.png", kit), write_file(p, "x");
}

static int detect(const char *rel, CarProject *c, char *why)
{
    char dir[1024];
    path_in(dir, sizeof dir, rel, "");
    why[0] = '\0';
    return car_project_detect(dir, c, why, 400);
}

static void check_kit_root_and_coche(void)
{
    static CarProject c;
    char why[400], want[1024];
    CHECK_EQ_INT(detect("kit", &c, why), 1);
    path_in(want, sizeof want, "kit", "");
    CHECK(strcmp(c.root, want) == 0);
    path_in(want, sizeof want, "kit/coche", "");
    CHECK(strcmp(c.piece, want) == 0);
    CHECK(strcmp(c.kit, "kit") == 0);
    CHECK(strcmp(c.info.id, "carrera-84") == 0 && strcmp(c.info.stem, "C84") == 0 && c.info.type == MOD_TYPE_CAR);
    CHECK(strcmp(c.info.name, "911 Carrera 3.2") == 0 && c.info.paint_count == 2);
    CHECK(strstr(c.cfm, "coche/SIMDATA/CARFAMS/C84.CFM") != NULL);
    CHECK(c.has_masks_dir && c.mask_count == 3);   /* the PNG files only, by name */
    CHECK(strcmp(c.mask[0], "bott_mask.png") == 0 && strcmp(c.mask[1], "frnt_mask.png") == 0 && strcmp(c.mask[2], "rsid_mask.png") == 0);
    CHECK(c.has_modelo && c.texture_count == 3 && c.far_count == 2 && c.kit_mask_count == 2);
    CHECK(strcmp(c.texture[0], "0001_topv.png") == 0 && strcmp(c.texture[2], "0010_rsid.png") == 0);
    CHECK(strcmp(c.kit_mask[0], "frnt.png") == 0);

    /* given coche/, with a modelo/ above it: the same kit */
    static CarProject d;
    CHECK_EQ_INT(detect("kit/coche", &d, why), 1);
    CHECK(strcmp(d.root, c.root) == 0 && strcmp(d.piece, c.piece) == 0 && d.texture_count == 3 && strcmp(d.kit, "kit") == 0);
    CHECK_EQ_INT(detect("kit/coche/", &d, why), 1);   /* a trailing slash */
    CHECK(strcmp(d.piece, c.piece) == 0);
    char dir[1024];
    path_in(dir, sizeof dir, "kit/coche/../coche", "");   /* a path that goes round */
    CHECK_EQ_INT(car_project_detect(dir, &d, why, sizeof why), 1);
    CHECK_EQ_INT(car_project_present(dir), 1);
    path_in(dir, sizeof dir, "kit", "");
    CHECK_EQ_INT(car_project_present(dir), 1);
}

static void check_a_lone_piece(void)
{
    static CarProject c;
    char why[400];
    make_dir("lone");
    write_file("lone/mod.ini", CARD);
    make_dir("lone/SIMDATA");
    make_dir("lone/SIMDATA/CARFAMS");
    write_file("lone/SIMDATA/CARFAMS/C84.CFM", "cfm");
    CHECK_EQ_INT(detect("lone", &c, why), 1);   /* a car's folder with no kit round it: its own root, no modelo/ */
    CHECK(strcmp(c.root, c.piece) == 0 && !c.has_modelo && c.texture_count == 0 && !c.has_masks_dir && c.mask_count == 0);
    CHECK(strcmp(c.kit, "lone") == 0);
}

static void check_what_is_not_a_car(void)
{
    static CarProject c;
    char why[400];
    make_dir("empty");
    CHECK_EQ_INT(detect("empty", &c, why), 0);
    CHECK(strstr(why, "empty") != NULL && strstr(why, "mod.ini") != NULL && strstr(why, "coche") != NULL);   /* what it looked at and what the person can pick */
    CHECK_EQ_INT(c.info.id[0], 0);
    char dir[1024];
    path_in(dir, sizeof dir, "empty", "");
    CHECK_EQ_INT(car_project_present(dir), 0);
    CHECK_EQ_INT(detect("nowhere", &c, why), 0);
    CHECK(strstr(why, "not a folder") != NULL);

    make_dir("track");   /* a mod.ini that is a track */
    write_file("track/mod.ini", "id=t\nname=T\nversion=1\ntype=track\nstem=TT\n");
    CHECK_EQ_INT(detect("track", &c, why), 0);
    CHECK(strstr(why, "type=track") != NULL && strstr(why, "type=car") != NULL);
    make_dir("tweak");   /* no type: a tweak */
    write_file("tweak/mod.ini", "id=t\nname=T\nversion=1\n");
    CHECK_EQ_INT(detect("tweak", &c, why), 0);
    CHECK(strstr(why, "type=car") != NULL);
    make_dir("broken");   /* a card the game would refuse: the game's own words */
    write_file("broken/mod.ini", "name=A\nversion=1\ntype=car\n");
    CHECK_EQ_INT(detect("broken", &c, why), 0);
    CHECK(strstr(why, "no id") != NULL);
    make_dir("nostem");
    write_file("nostem/mod.ini", "id=a\nname=A\nversion=1\ntype=car\n");
    CHECK_EQ_INT(detect("nostem", &c, why), 0);
    CHECK(strstr(why, "stem=") != NULL);
    make_dir("nocfm");   /* a car card with no car file */
    write_file("nocfm/mod.ini", CARD);
    CHECK_EQ_INT(detect("nocfm", &c, why), 0);
    CHECK(strstr(why, "C84.CFM") != NULL && strstr(why, "SIMDATA/CARFAMS") != NULL);
    for (const char *p = why; *p != '\0'; p++) CHECK((unsigned char)*p >= 0x20 || *p == '\n');
    make_dir("nokit");   /* coche/ that is not a car, and a kit around it */
    make_dir("nokit/coche");
    make_dir("nokit/modelo");
    CHECK_EQ_INT(detect("nokit", &c, why), 0);
    CHECK(strstr(why, "nokit") != NULL && strstr(why, "coche/mod.ini") != NULL);
    CHECK_EQ_INT(detect("kit/modelo", &c, why), 0);   /* a folder of the kit that is not the car */
    CHECK_EQ_INT(detect("kit/coche/car_masks", &c, why), 0);
    CHECK_EQ_INT(car_project_detect(NULL, &c, why, sizeof why), 0);
}

static void check_no_masks_is_a_car(void)
{
    static CarProject c;
    char why[400];
    make_kit("bare");
    char p[1024];
    path_in(p, sizeof p, "bare/coche/car_masks/C84/rsid_mask.png", "");
    remove(p);
    path_in(p, sizeof p, "bare/coche/car_masks/C84/frnt_mask.png", "");
    remove(p);
    path_in(p, sizeof p, "bare/coche/car_masks/C84/bott_mask.png", "");
    remove(p);
    path_in(p, sizeof p, "bare/coche/car_masks/C84/notes.txt", "");
    remove(p);
    path_in(p, sizeof p, "bare/coche/car_masks/C84", "");
    rmdir(p);
    CHECK_EQ_INT(detect("bare", &c, why), 1);   /* no masks yet is listed as none, not refused */
    CHECK(!c.has_masks_dir && c.mask_count == 0);
}

static void check_long_lists(void)
{
    static CarProject c;
    char why[400], p[256];
    make_kit("big");
    for (int i = 0; i < CAR_PROJECT_LIST + 6; i++) {
        snprintf(p, sizeof p, "big/modelo/c1/%04d_t%02d.png", i, i);
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
    make_kit("kit");
    check_kit_root_and_coche();
    check_a_lone_piece();
    check_what_is_not_a_car();
    check_no_masks_is_a_car();
    check_long_lists();
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
