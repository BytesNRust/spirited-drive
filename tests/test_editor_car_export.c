/* Export original car (editor/car_export.c): each kind of car made into a project from a fixture game folder (synthetic car files and masks, no game data), and
 * the repository's own data/car_masks/ as the source of the masks for every car: the folders are spelt as the repository spells them, the files keep their names,
 * their bytes and so their sizes. argv[1] is the repository's data/ folder. */
#include "car_export.h"
#include "car_project.h"
#include "formats/car_piece.h"
#include "formats/mod_ini.h"
#include "formats/pbs.h"
#include "game/race/car_paint.h"
#include "test.h"

#include <limits.h>
#include <dirent.h>
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[512];

static void path_in(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", base, rel);
}

/* `text` without the fixture's folder in it: a word the windows never say is looked for in what they say, not in mkdtemp's random letters (they made "CD" once) */
static const char *without_base(const char *text)
{
    static char out[1024];
    size_t at = 0;
    const size_t len = strlen(base);
    for (const char *p = text; *p != '\0' && at + 1 < sizeof out;) {
        if (len > 0 && strncmp(p, base, len) == 0) p += len;
        else out[at++] = *p++;
    }
    out[at] = '\0';
    return out;
}

static void make_dirs(const char *rel)
{
    char p[1024];
    path_in(p, sizeof p, rel);
    for (char *s = p + strlen(base) + 1; *s != '\0'; ++s)
        if (*s == '/') {
            *s = '\0';
            mkdir(p, 0777);
            *s = '/';
        }
    mkdir(p, 0777);
}

static void write_bytes(const char *rel, const uint8_t *data, size_t size)
{
    char p[1024];
    path_in(p, sizeof p, rel);
    FILE *f = fopen(p, "wb");
    if (f != NULL) fwrite(data, 1, size, f), fclose(f);
}

static void write_text(const char *rel, const char *text)
{
    write_bytes(rel, (const uint8_t *)text, strlen(text));
}

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = malloc(1 << 20);
    *size = data != NULL ? fread(data, 1, 1 << 20, f) : 0;
    fclose(f);
    return data;
}

static int exists(const char *rel)
{
    char p[1024];
    struct stat st;
    path_in(p, sizeof p, rel);
    return lstat(p, &st) == 0;
}

/* a synthetic mask: a PNG header (8-bit grey, w x h) and `fill` filler bytes: the export copies files, it never decodes them */
static void write_mask(const char *rel, uint32_t w, uint32_t h, uint8_t fill)
{
    uint8_t png[64] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 'I', 'H', 'D', 'R' };
    png[16] = (uint8_t)(w >> 24), png[17] = (uint8_t)(w >> 16), png[18] = (uint8_t)(w >> 8), png[19] = (uint8_t)w;
    png[20] = (uint8_t)(h >> 24), png[21] = (uint8_t)(h >> 16), png[22] = (uint8_t)(h >> 8), png[23] = (uint8_t)h;
    png[24] = 8, png[25] = 0;
    memset(png + 26, fill, sizeof png - 26);
    write_bytes(rel, png, sizeof png);
}

static void same_file(const char *a_rel, const char *b_rel, const char *what)
{
    char a[1024], b[1024];
    size_t na = 0, nb = 0;
    path_in(a, sizeof a, a_rel);
    path_in(b, sizeof b, b_rel);
    uint8_t *da = read_all(a, &na), *db = read_all(b, &nb);
    if (!(da != NULL && db != NULL && na == nb && memcmp(da, db, na) == 0)) fprintf(stderr, "differs: %s\n", what);
    CHECK(da != NULL && db != NULL && na == nb && memcmp(da, db, na) == 0);
    free(da);
    free(db);
}

/* the fixture game: cd/ (a car file for every car, the 911's PDN and PBS) and data/ (masks for some of the cars) */
static void make_game(void)
{
    make_dirs("game/cd/SIMDATA/CARFAMS");
    make_dirs("game/cd/SIMDATA/CARSPECS");
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        char rel[256];
        snprintf(rel, sizeof rel, "game/cd/SIMDATA/CARFAMS/%s.CFM", car_export_car(i)->file);
        if (strcmp(car_export_car(i)->file, "COPMUST") == 0) snprintf(rel, sizeof rel, "game/cd/SIMDATA/CARFAMS/CopMust.CFM");   /* the disc's spelling is not ours */
        write_text(rel, car_export_car(i)->label);
    }
    write_text("game/cd/SIMDATA/CARFAMS/P911.PDN", "pdn of the 911");
    uint8_t pbs[PBS_SIZE] = { 0 };   /* a raw record: ABS on, traction control off */
    pbs[CAR_PIECE_PBS_ABS] = 1;
    write_bytes("game/cd/SIMDATA/CARSPECS/P911.PBS", pbs, sizeof pbs);
    make_dirs("game/data/car_masks/P911");
    make_dirs("game/data/car_masks/Wagon");
    make_dirs("game/data/car_masks/COPMUST");
    make_dirs("game/data/car_masks/TSupra");
    make_dirs("game/data/dash_masks/P911");
    write_mask("game/data/car_masks/P911/side_mask.png", 128, 64, 1);
    write_mask("game/data/car_masks/P911/frnt_lo_mask.png", 64, 32, 2);
    write_mask("game/data/car_masks/P911/frnt.before-1603_mask.png", 64, 32, 9);   /* an old copy: not exported */
    write_text("game/data/car_masks/P911/notes.txt", "not a mask");
    write_mask("game/data/dash_masks/P911/dash_mask.png", 32, 32, 3);
    write_mask("game/data/car_masks/Wagon/side_mask.png", 100, 50, 4);
    write_mask("game/data/car_masks/Wagon/rear_mask.png", 48, 24, 5);
    write_mask("game/data/car_masks/COPMUST/bott_mask.png", 16, 16, 6);
    write_mask("game/data/car_masks/TSupra/frnt_mask.png", 8, 8, 7);
}

static int index_of(const char *folder)
{
    for (int i = 0; car_export_car(i) != NULL; ++i)
        if (strcmp(car_export_car(i)->folder, folder) == 0) return i;
    return -1;
}

/* 1 when the text file has a line that is exactly `line` */
static int has_line(const char *rel, const char *line)
{
    char p[1024];
    size_t n = 0;
    path_in(p, sizeof p, rel);
    uint8_t *d = read_all(p, &n);
    int found = 0;
    if (d != NULL && n < (1 << 20)) {
        d[n] = '\0';
        const size_t len = strlen(line);
        for (const char *s = (const char *)d; s != NULL && !found;) {
            const char *nl = strchr(s, '\n');
            found = (nl != NULL ? (size_t)(nl - s) : strlen(s)) == len && strncmp(s, line, len) == 0;
            s = nl != NULL ? nl + 1 : NULL;
        }
    }
    free(d);
    return found;
}

static int has_substring(const char *rel, const char *text)
{
    char p[1024];
    size_t n = 0;
    path_in(p, sizeof p, rel);
    uint8_t *d = read_all(p, &n);
    int found = 0;
    if (d != NULL && n < (1 << 20)) {
        d[n] = '\0';
        found = strstr((const char *)d, text) != NULL;
    }
    free(d);
    return found;
}

/* a catalogue colour name longer than a card's (the Diablo's "30th Anniversary Lavender" is 25 characters, the card holds 24): the export cuts it, and says which */
static void test_cut_names(void)
{
    int diablo = 0;
    char text[400];
    for (int i = 0; i < EXPORT_CARS; ++i) {
        const ExportCar *c = car_export_car(i);
        int longer = 0;
        for (int k = 0; c->kind == EXPORT_DRIVABLE && k < car_paint_count(c->model); ++k) longer += (int)strlen(car_paint(c->model, k)->name) > MOD_INI_PAINT_NAME_MAX;
        CHECK_EQ_INT(car_export_cut_names(i, text, sizeof text), longer);
        CHECK((longer > 0) == (text[0] != '\0'));
        if (strcmp(c->file, "LDIABL") == 0) {
            diablo = 1;
            CHECK_EQ_INT(longer, 1);
            CHECK(strstr(text, "\"30th Anniversary Lavender\" is \"30th Anniversary Lavende\" on the card") != NULL);
            CHECK(strstr(text, "24 characters at most") != NULL);
        }
    }
    CHECK(diablo);
    CHECK_EQ_INT(car_export_cut_names(-1, text, sizeof text), 0);
    CHECK_EQ_INT(car_export_cut_names(0, NULL, 0) >= 0, 1);
}

static void test_table(const char *repo_data)
{
    CHECK_EQ_INT(EXPORT_CARS, 21);
    CHECK(car_export_car(EXPORT_CARS) == NULL && car_export_car(-1) == NULL);
    int seen_model[8] = { 0 };
    for (int i = 0; i < EXPORT_CARS; ++i) {
        const ExportCar *c = car_export_car(i);
        CHECK(c != NULL);
        CHECK_EQ_INT(c->kind, i < 8 ? EXPORT_DRIVABLE : i < 20 ? EXPORT_TRAFFIC : EXPORT_POLICE);
        if (c->kind == EXPORT_DRIVABLE) {
            CHECK(c->model >= 0 && c->model < 8 && !seen_model[c->model]);
            seen_model[c->model] = 1;
            CHECK(car_paint_count(c->model) > 0);
        } else {
            CHECK_EQ_INT(c->model, -1);
        }
        /* the folder is spelt exactly as the repository's data/car_masks/ spells it (a disk that ignores case would not say), but for the car the game has no masks for */
        char dir[1024];
        snprintf(dir, sizeof dir, "%s/car_masks", repo_data);
        DIR *d = opendir(dir);
        int exact = 0;
        for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) exact |= strcmp(e->d_name, c->folder) == 0;
        if (d != NULL) closedir(d);
        if (strcmp(c->folder, "probe") == 0) CHECK(!exact);
        else {
            if (!exact) fprintf(stderr, "no folder spelt %s in %s\n", c->folder, dir);
            CHECK(exact);
        }
    }
    char name[96];
    car_export_folder_name(index_of("P911"), name, sizeof name);
    CHECK(strcmp(name, "p911-original") == 0);
    car_export_folder_name(index_of("Wagon"), name, sizeof name);
    CHECK(strcmp(name, "wagon-masks") == 0);
    car_export_folder_name(index_of("COPMUST"), name, sizeof name);
    CHECK(strcmp(name, "copmust-masks") == 0);
    car_export_folder_name(99, name, sizeof name);
    CHECK(name[0] == '\0');
    for (int i = 0; i < EXPORT_CARS; ++i) {
        car_export_folder_name(i, name, sizeof name);
        CHECK(mod_ini_id_ok(name));
    }
    car_export_repo_path("Wagon", name, sizeof name);
    CHECK(strcmp(name, "data/car_masks/Wagon/") == 0);
    char url[128];
    CHECK(car_export_file_url("/Users/a b/\xc3\xa9", url, sizeof url));
    CHECK(strcmp(url, "file:///Users/a%20b/%C3%A9") == 0);
    CHECK(!car_export_file_url("/Users/a b/\xc3\xa9", url, 20));
}

static void test_source(void)
{
    ExportSource s;
    char root[1024], app[1100], tree[1100], want[1200], player[1100], pref[1100];
    path_in(root, sizeof root, "src");
    make_dirs("src/Spirited Drive.app/Contents/MacOS");
    make_dirs("src/Spirited Drive.app/Contents/Resources/data/car_masks");
    make_dirs("src/tree/data/car_masks");
    make_dirs("src/player/gamedata/SIMDATA/CARFAMS");
    make_dirs("src/pref/gamedata/SIMDATA/CARFAMS");
    snprintf(app, sizeof app, "%s/Spirited Drive.app/Contents/MacOS/tnfs", root);
    snprintf(tree, sizeof tree, "%s/tree/tnfs", root);
    snprintf(player, sizeof player, "%s/player", root);
    snprintf(pref, sizeof pref, "%s/pref/", root);
    car_export_source(app, player, pref, NULL, NULL, &s);
    snprintf(want, sizeof want, "%s/Spirited Drive.app/Contents/Resources/data", root);
    CHECK(strcmp(s.data, want) == 0);
    snprintf(want, sizeof want, "%s/gamedata", player);
    CHECK(strcmp(s.cd, want) == 0);   /* --user wins over the system's folder */
    car_export_source(tree, "", pref, NULL, NULL, &s);
    snprintf(want, sizeof want, "%s/tree/data", root);
    CHECK(strcmp(s.data, want) == 0);
    snprintf(want, sizeof want, "%sgamedata", pref);
    CHECK(strcmp(s.cd, want) == 0);
    car_export_source(tree, "", pref, "/x/cd", "/x/data", &s);
    CHECK(strcmp(s.cd, "/x/cd") == 0 && strcmp(s.data, "/x/data") == 0);
    car_export_source("/nowhere/tnfs", "", NULL, NULL, NULL, &s);   /* nothing is there: the first candidate, for the message */
    CHECK(strcmp(s.data, "/nowhere/data") == 0);
}

static ExportSource fixture_source(void)
{
    ExportSource s;
    path_in(s.cd, sizeof s.cd, "game/cd");
    path_in(s.data, sizeof s.data, "game/data");
    return s;
}

static void test_available(void)
{
    ExportSource s = fixture_source();
    char why[400];
    CHECK(car_export_available(&s, index_of("P911"), why, sizeof why));
    CHECK(car_export_available(&s, index_of("Wagon"), why, sizeof why));
    CHECK(car_export_available(&s, index_of("COPMUST"), why, sizeof why));   /* CopMust.CFM on the disc, COPMUST here */
    CHECK(!car_export_available(&s, index_of("probe"), why, sizeof why));
    CHECK(strstr(why, "no masks") != NULL && strstr(why, "probe") != NULL);
    CHECK(strstr(without_base(why), "CD") == NULL);   /* the windows never say "the CD" (the fixture's folder may have the letters) */
    CHECK(!car_export_available(&s, index_of("CRX"), why, sizeof why));   /* its masks folder is not there */
    CHECK(!car_export_available(&s, 99, why, sizeof why));
    ExportSource none = s;
    snprintf(none.cd, sizeof none.cd, "%s", "/nowhere");
    CHECK(!car_export_available(&none, index_of("P911"), why, sizeof why));
    CHECK(strstr(why, "SIMDATA/CARFAMS/P911.CFM") != NULL && strstr(why, "/nowhere") != NULL);
    make_dirs("game/data/car_masks/PICKUP");   /* a folder with no mask in it */
    CHECK(!car_export_available(&s, index_of("Pickup"), why, sizeof why));
    CHECK(strstr(why, "holds none") != NULL);
}

static void test_drivable(void)
{
    ExportSource s = fixture_source();
    char why[400], dir[1024];
    path_in(dir, sizeof dir, "out/p911-original");
    CHECK(car_export(&s, index_of("P911"), dir, why, sizeof why));
    CHECK(!exists("out/p911-original.exporting"));
    same_file("game/cd/SIMDATA/CARFAMS/P911.CFM", "out/p911-original/SIMDATA/CARFAMS/P911.CFM", "CFM");
    same_file("game/cd/SIMDATA/CARFAMS/P911.PDN", "out/p911-original/SIMDATA/CARFAMS/P911.PDN", "PDN");
    same_file("game/cd/SIMDATA/CARSPECS/P911.PBS", "out/p911-original/SIMDATA/CARSPECS/P911.PBS", "PBS");
    /* the masks: the same names, the same bytes (so the same sizes), no old copy, no stray file */
    same_file("game/data/car_masks/P911/side_mask.png", "out/p911-original/car_masks/P911/side_mask.png", "side mask");
    same_file("game/data/car_masks/P911/frnt_lo_mask.png", "out/p911-original/car_masks/P911/frnt_lo_mask.png", "frnt_lo mask");
    CHECK(!exists("out/p911-original/car_masks/P911/frnt.before-1603_mask.png") && !exists("out/p911-original/car_masks/P911/notes.txt"));
    same_file("game/data/dash_masks/P911/dash_mask.png", "out/p911-original/dash_masks/P911/dash_mask.png", "dash mask");
    CHECK(!exists("out/p911-original/project.ini"));   /* a normal car project: its card is mod.ini */
    /* the card: what the game parses, redistributable=0, the car's own stem, the catalogue colours */
    char card_path[1100];
    path_in(card_path, sizeof card_path, "out/p911-original/mod.ini");
    size_t n = 0;
    uint8_t *card = read_all(card_path, &n);
    ModInfo info;
    char reason[200];
    CHECK(card != NULL && mod_ini_parse(card, n, &info, reason, sizeof reason));
    CHECK_EQ_INT(info.type, MOD_TYPE_CAR);
    CHECK(strcmp(info.stem, "P911") == 0 && strcmp(info.id, "p911-original") == 0);
    CHECK(strcmp(info.sound_from, "911") == 0 && strcmp(info.dash_from, "911") == 0);
    CHECK(strcmp(info.assist_abs, "yes") == 0 && strcmp(info.assist_traction, "no") == 0);
    CHECK_EQ_INT(info.paint_count, car_paint_count(2));
    for (int k = 0; k < info.paint_count && k < car_paint_count(2); ++k) {
        CHECK(strcmp(info.paints[k].name, car_paint(2, k)->name) == 0);
        CHECK(memcmp(info.paints[k].rgb, car_paint(2, k)->rgb, 3) == 0);
    }
    free(card);
    CHECK(has_line("out/p911-original/mod.ini", "redistributable=0"));
    CHECK(has_substring("out/p911-original/mod.ini", "do not share"));
    /* it opens as a car project, its masks listed by the folder's own spelling */
    CarProject c;
    CHECK(car_project_detect(dir, &c, why, sizeof why));
    CHECK(!c.masks_only && strcmp(c.folder, "P911") == 0 && c.has_masks_dir && c.mask_count == 2);
    /* a second export into the same folder is refused, and changes nothing */
    write_text("out/p911-original/marker.txt", "mine");
    /* what the owner edited in the kit (a longer card of colours, a PSD of his, a mask he painted) is as he left it: the refusal writes nothing, not even the files the export would make the same */
    write_text("out/p911-original/mod.ini", "id=p911-original\nstem=P911\npaints=A:111111,B:222222,C:333333,D:444444,E:555555,F:666666,G:777777,H:888888\n");
    write_text("out/p911-original/side.psd", "my own psd");
    write_text("out/p911-original/car_masks/P911/frnt_mask.png", "my own mask");
    CHECK(!car_export(&s, index_of("P911"), dir, why, sizeof why));
    CHECK(strstr(why, "already there") != NULL);
    CHECK(has_substring("out/p911-original/mod.ini", "paints=A:111111,B:222222,C:333333,D:444444,E:555555,F:666666,G:777777,H:888888"));
    CHECK(has_line("out/p911-original/side.psd", "my own psd") && has_line("out/p911-original/car_masks/P911/frnt_mask.png", "my own mask"));
    CHECK(exists("out/p911-original/marker.txt") && !exists("out/p911-original.exporting"));
    /* a car with a mixed-case masks folder: the project keeps the folder's spelling, the stem is the card's */
    write_text("game/cd/SIMDATA/CARFAMS/TSUPRA.PDN", "pdn of the Supra");
    write_text("game/cd/SIMDATA/CARSPECS/TSUPRA.PBS", "");   /* a PBS that is none: the car has neither assist */
    char dir2[1024];
    path_in(dir2, sizeof dir2, "out/tsupra-original");
    CHECK(car_export(&s, index_of("TSupra"), dir2, why, sizeof why));
    CHECK(exists("out/tsupra-original/car_masks/TSupra/frnt_mask.png"));
    CHECK(car_project_detect(dir2, &c, why, sizeof why));
    CHECK(strcmp(c.info.stem, "TSUPRA") == 0 && strcmp(c.folder, "TSupra") == 0 && c.has_masks_dir && c.mask_count == 1);
    CHECK(has_line("out/tsupra-original/mod.ini", "abs=no") && has_line("out/tsupra-original/mod.ini", "traction=no"));
}

static void check_masks_project(const char *rel_dir, const char *folder, const char *cfm, const char *id)
{
    char dir[1100], why[300], line[160], src[200], dst[200];
    path_in(dir, sizeof dir, rel_dir);
    CarProject c;
    CHECK(car_project_is_masks(dir) && car_project_present(dir));
    CHECK(car_project_detect(dir, &c, why, sizeof why));
    CHECK(c.masks_only && strcmp(c.folder, folder) == 0 && strcmp(c.info.id, id) == 0 && c.has_masks_dir);
    snprintf(line, sizeof line, "%s/project.ini", rel_dir);
    CHECK(has_line(line, "kind=masks") && has_line(line, "redistributable=0"));
    snprintf(src, sizeof src, "masks=%s", folder);
    CHECK(has_line(line, src));
    snprintf(dst, sizeof dst, "%s/original/%s", rel_dir, cfm);
    CHECK(exists(dst));
    snprintf(dst, sizeof dst, "%s/mod.ini", rel_dir);
    CHECK(!exists(dst));   /* no card: there is no PBS to describe */
}

static void test_masks_projects(void)
{
    ExportSource s = fixture_source();
    char why[400], dir[1024];
    path_in(dir, sizeof dir, "out/wagon-masks");
    CHECK(car_export(&s, index_of("Wagon"), dir, why, sizeof why));
    check_masks_project("out/wagon-masks", "Wagon", "WAGON.CFM", "wagon-masks");
    same_file("game/data/car_masks/Wagon/side_mask.png", "out/wagon-masks/car_masks/Wagon/side_mask.png", "wagon side");
    same_file("game/data/car_masks/Wagon/rear_mask.png", "out/wagon-masks/car_masks/Wagon/rear_mask.png", "wagon rear");
    same_file("game/cd/SIMDATA/CARFAMS/WAGON.CFM", "out/wagon-masks/original/WAGON.CFM", "wagon CFM");
    CHECK(has_substring("out/wagon-masks/project.ini", "export_sizes=rear_mask.png:48x24 side_mask.png:100x50"));   /* the sizes of what was exported, by name */
    CHECK(!exists("out/wagon-masks/SIMDATA") && !exists("out/wagon-masks/dash_masks"));
    CarProject c;
    path_in(dir, sizeof dir, "out/wagon-masks");
    CHECK(car_project_detect(dir, &c, why, sizeof why) && c.mask_count == 2 && strcmp(c.mask[0], "rear_mask.png") == 0);
    /* the police car: the disc spells its file CopMust.CFM, the folder is COPMUST */
    char dir2[1024];
    path_in(dir2, sizeof dir2, "out/copmust-masks");
    CHECK(car_export(&s, index_of("COPMUST"), dir2, why, sizeof why));
    check_masks_project("out/copmust-masks", "COPMUST", "CopMust.CFM", "copmust-masks");
    /* the car with no masks is refused and nothing is left */
    char dir3[1024];
    path_in(dir3, sizeof dir3, "out/probe-masks");
    CHECK(!car_export(&s, index_of("probe"), dir3, why, sizeof why));
    CHECK(strstr(why, "no masks") != NULL);
    CHECK(!exists("out/probe-masks") && !exists("out/probe-masks.exporting"));
    /* a second export into the same folder is refused */
    CHECK(!car_export(&s, index_of("Wagon"), dir, why, sizeof why));
    CHECK(strstr(why, "already there") != NULL);
    /* a mask that is no picture stops the export and leaves nothing behind */
    write_text("game/data/car_masks/Wagon/broken_mask.png", "not a png");
    char dir4[1024];
    path_in(dir4, sizeof dir4, "out/wagon-again");
    CHECK(!car_export(&s, index_of("Wagon"), dir4, why, sizeof why));
    CHECK(strstr(why, "not a PNG") != NULL);
    CHECK(!exists("out/wagon-again") && !exists("out/wagon-again.exporting"));
    /* an unfinished export of another time is not overwritten */
    make_dirs("out/stale.exporting");
    char dir5[1024];
    path_in(dir5, sizeof dir5, "out/stale");
    CHECK(!car_export(&s, index_of("COPMUST"), dir5, why, sizeof why));
    CHECK(strstr(why, "did not finish") != NULL);
}

/* every car of the repository's data/car_masks/ exported from its own masks: the project's folder holds exactly what the game's does */
static void test_repository_masks(const char *repo_data)
{
    ExportSource s;
    snprintf(s.data, sizeof s.data, "%s", repo_data);
    path_in(s.cd, sizeof s.cd, "game/cd");
    int exported = 0;
    for (int i = 0; i < EXPORT_CARS; ++i) {
        const ExportCar *car = car_export_car(i);
        char dir[1024], why[400], rel[256];
        snprintf(rel, sizeof rel, "repo/%s", car->folder);
        path_in(dir, sizeof dir, rel);
        if (car->kind == EXPORT_DRIVABLE) continue;   /* a drivable car needs its PDN and PBS, which the fixture has for the 911 only */
        const int ok = car_export(&s, i, dir, why, sizeof why);
        CHECK(ok == (strcmp(car->folder, "probe") != 0));
        if (!ok) continue;
        exported++;
        char src[1024], dst[1024];
        snprintf(src, sizeof src, "%s/car_masks/%s", repo_data, car->folder);
        snprintf(dst, sizeof dst, "%s/car_masks/%s", dir, car->folder);
        DIR *d = opendir(src);
        int copied = 0;
        for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) {
            const size_t len = strlen(e->d_name);
            if (len < 10 || strcmp(e->d_name + len - 9, "_mask.png") != 0 || strchr(e->d_name, '.') != e->d_name + len - 4) continue;   /* the files the game reads */
            char a[1100], b[1100];
            snprintf(a, sizeof a, "%s/%s", src, e->d_name);
            snprintf(b, sizeof b, "%s/%s", dst, e->d_name);
            size_t na = 0, nb = 0;
            uint8_t *da = read_all(a, &na), *db = read_all(b, &nb);
            CHECK(da != NULL && db != NULL && na == nb && memcmp(da, db, na) == 0);
            free(da);
            free(db);
            copied++;
        }
        if (d != NULL) closedir(d);
        CarProject c;
        CHECK(car_project_detect(dir, &c, why, sizeof why) && c.mask_count == (copied < CAR_PROJECT_LIST ? copied : c.mask_count));
        CHECK(copied > 0);
    }
    CHECK_EQ_INT(exported, 12);   /* eleven traffic cars and the police car; probe has no masks */
}

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_editor_car_export REPO_DATA_DIR\n");
        return 2;
    }
    snprintf(base, sizeof base, "%s/car_export_test_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(base) == NULL) return 2;
    char real[PATH_MAX];   /* realpath wants PATH_MAX bytes (Linux: 4096; glibc stops the program otherwise) */
    if (realpath(base, real) != NULL) snprintf(base, sizeof base, "%s", real);
    test_table(argv[1]);
    test_cut_names();
    make_game();
    test_source();
    test_available();
    test_drivable();
    test_masks_projects();
    test_repository_masks(argv[1]);
    nftw(base, remove_entry, 16, FTW_DEPTH | FTW_PHYS);   /* the scratch folder this test made, and only it */
    return TEST_REPORT();
}
