/* dep59 step 1: the added cars come from the mod folders in effect (game/sys/car_pieces.h). A folder whose mod.ini says type=car and
 * whose files pass car_piece_check is a car, in Restomod only; one that does not is refused with one log line, and a folder of another
 * type, or with no mod.ini, is passed over. A reload keeps the numbers of the cars that stay; car_pieces_follow loads again only when the
 * mods in effect changed (mod_set_generation). The pieces are copies of the CD's Viper (skipped without the CD, as test_models). */
#include "formats/car_ai.h"
#include "formats/crc32.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "formats/png.h"
#include "game/race/car_registry.h"
#include "game/sys/car_pieces.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_profile.h"
#include "game/sys/piece_print.h"
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

/* a w x h PNG `name` in `dir` (dep59 step 3: a piece's pictures and skins are of their exact sizes) */
static void write_png(const char *dir, const char *name, int w, int h)
{
    uint8_t *px = calloc((size_t)w * h, 1), pal[768] = { 0 }, *png = NULL;
    size_t len = 0;
    char path[1200];
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = px != NULL && png_write_indexed(px, (uint32_t)w, (uint32_t)h, pal, &png, &len) == PNG_OK ? fopen(path, "wb") : NULL;
    if (f != NULL) fwrite(png, 1, len, f), fclose(f);
    png_free(png), free(px);
}

/* a piece folder <base>/<folder> of id `id` and stem `stem`: its card (`extra` lines after it) and the Viper's files, those of `leave`
 * ("CFM", "PDN", "PBS" or "") left out; its path in `out` */
static int piece(const char *folder, const char *id, const char *stem, const char *extra, const char *leave, char out[400])
{
    char dir[400], card[600], src[600], name[64];
    snprintf(out, 400, "%s/%s", base, folder);
    snprintf(card, sizeof card, "id=%s\nname=%s\nversion=1\ntype=car\nstem=%s\nshort_name=T\n%s", id, id, stem, extra);
    write_text(out, "mod.ini", card);
    write_png(out, "card.png", 309, 166);   /* dep59 step 3: every piece brings its card */
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

static const char FROM[] = "sound_from=viper\n";

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
    CHECK_EQ_INT(car_registry_ai_model(16), -1);   /* 3b: no original's AI, its own (the Viper's PDN: 60026, coast 4) */
    CHECK(car_registry_ai(16) != NULL && car_registry_ai(16)->pace == 60026 && car_registry_ai(16)->coast == CAR_AI_COAST_DEFAULT);
    CHECK(car_registry_piece(16) != NULL && car_registry_piece(16)->card_name_off == 0);   /* its name on its card by default */
    char b[400];   /* card_name=off: none */
    CHECK(piece("b", "named-car", "NAMECAR", "sound_from=viper\ncard_name=off\n", "", b));
    CHECK_EQ_INT(load1(b, 1), 1);
    CHECK(car_registry_piece(16) != NULL && car_registry_piece(16)->card_name_off == 1);
    CHECK_EQ_INT(load1(a, 1), 1);   /* again: the same */
    CHECK_EQ_INT(car_registry_find("test-car"), 16);
    CHECK_EQ_INT(load1(a, 0), 0);   /* not Restomod: none */
    CHECK_EQ_INT(car_registry_count(), 0);
}

static void test_refused_and_passed_over(void)
{
    char d[400];
    static const struct { const char *folder, *extra, *leave; } BAD[] = {
        { "no_pdn", "sound_from=viper\n", "PDN" }, { "no_pbs", "sound_from=viper\n", "PBS" },
        { "no_cfm", "sound_from=viper\n", "CFM" }, { "lada", "sound_from=lada\n", "" },
        { "ai_from", "sound_from=viper\nai_from=viper\n", "" }, { "turbo", "sound_from=viper\nai_level=turbo\n", "" },   /* 3b */
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        CHECK(piece(BAD[i].folder, "bad", "BADCAR", BAD[i].extra, BAD[i].leave, d));
        car_registry_clear();
        CHECK_EQ_INT(load1(d, 1), 0);
        CHECK_EQ_INT(car_registry_count(), 0);
    }
    snprintf(d, sizeof d, "%s/no_stem", base);   /* no stem */
    write_text(d, "mod.ini", "id=x\nname=X\nversion=1\ntype=car\nsound_from=viper\n");
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
    CHECK(strcmp(car_registry_piece(16)->dir, a) == 0 && strcmp(car_registry_piece(17)->dir, b) == 0);   /* dep59 step 3: its folder, for its pictures */
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
    CHECK(car_registry_add(&(CarPiece){ .id = "stale", .stem = "STALE", .sound_model = 0, .ai = { CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, .dash_model = -1 }, NULL, 0) == 16);
    car_pieces_follow();
    CHECK_EQ_INT(car_registry_count(), 0);
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    car_pieces_follow();
    CHECK_EQ_INT(car_registry_find("follow-car"), 16);
    mod_set_stop();
}

/* `path`'s bytes, all of them, into a new buffer (*size); NULL when it cannot be read */
static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n > 0 ? (size_t)n : 1);
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = (size_t)n;
    return d;
}

static void spill(const char *path, const uint8_t *d, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(d, 1, size, f), fclose(f);
}

/* port (dep59 1b): a piece's fingerprint is the CRC-32 of its PBS then its PDN, as on disk, then (3b) its AI's pace and coast, 4 bytes LE
 * each: what decides how it drives, not its look */
static uint32_t fingerprint_of(const char *dir)
{
    car_registry_clear();
    CHECK_EQ_INT(load1(dir, 1), 1);
    const CarPiece *p = car_registry_piece(car_registry_find("print-car"));
    return p != NULL ? p->fingerprint : 0;
}

static const uint8_t *print_of(const char *dir)
{
    car_registry_clear();
    CHECK_EQ_INT(load1(dir, 1), 1);
    const CarPiece *p = car_registry_piece(car_registry_find("print-car"));
    return p != NULL ? p->print : NULL;
}

static void test_the_fingerprint(void)
{
    char a[400], path[600];
    CHECK(piece("print", "print-car", "PRINT", FROM, "", a));
    const uint32_t first = fingerprint_of(a);
    CHECK(first != 0);
    size_t pbs_len = 0, pdn_len = 0;   /* the print as the files and the pace make it */
    snprintf(path, sizeof path, "%s/SIMDATA/CARSPECS/PRINT.PBS", a);
    uint8_t *pbs_file = slurp(path, &pbs_len);
    snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/PRINT.PDN", a);
    uint8_t *pdn_file = slurp(path, &pdn_len), pdn[PDN_SIZE];
    CarAiTuning ai = { 0, 0 };
    char why[96];
    CHECK(pbs_file != NULL && pdn_file != NULL && pdn_read(pdn, pdn_file, pdn_len) && car_ai_tuning(pdn, NULL, &ai, why, sizeof why));
    uint8_t tail[8];
    for (int i = 0; i < 4; i++) tail[i] = (uint8_t)((uint32_t)ai.pace >> (8 * i)), tail[4 + i] = (uint8_t)((uint32_t)ai.coast >> (8 * i));
    const uint32_t made = crc32_update(crc32_update(crc32_update(0, pbs_file, pbs_len), pdn_file, pdn_len), tail, sizeof tail);
    CHECK_EQ_INT((int)first, (int)made);
    free(pbs_file), free(pdn_file);
    CHECK_EQ_INT((int)fingerprint_of(a), (int)first);   /* the same files, the same print */
    snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/PRINT.CFM", a);   /* the look changed: the same print */
    size_t size = 0;
    uint8_t *cfm = slurp(path, &size);
    CHECK(cfm != NULL && size > 100);
    if (cfm != NULL) cfm[size - 1] ^= 0x01, spill(path, cfm, size), free(cfm);
    CHECK_EQ_INT((int)fingerprint_of(a), (int)first);
    snprintf(path, sizeof path, "%s/SIMDATA/CARSPECS/PRINT.PBS", a);   /* the specs changed, still sealed: another print */
    uint8_t *pbs = slurp(path, &size);
    uint8_t record[PBS_SIZE];
    CHECK(pbs != NULL && pbs_read(record, pbs, size));
    free(pbs);
    record[0x40] ^= 0x01;   /* the word after power (+0x3c) */
    pbs_seal(record);
    spill(path, record, PBS_SIZE);
    const uint32_t changed = fingerprint_of(a);
    CHECK(changed != 0 && changed != first);
    uint8_t changed_print[PIECE_PRINT_SIZE];
    memcpy(changed_print, print_of(a), sizeof changed_print);   /* the print changes with the specs, as the CRC does */
    CHECK(!piece_print_is_none(changed_print));
    /* 3b: ai_level=top: the level's pair, and the print changes with the tuning alone (the files as they were) */
    char b[400];
    CHECK(piece("print_top", "print-car", "PRINT", "sound_from=viper\nai_level=top\n", "", b));
    CHECK(piece("print_none", "print-car", "PRINT", FROM, "", a));
    const uint32_t none = fingerprint_of(a), top = fingerprint_of(b);
    const CarAiTuning *top_ai = car_registry_ai(car_registry_find("print-car"));
    CHECK(top_ai != NULL && top_ai->pace == 65536 && top_ai->coast == 4);
    CHECK(none != 0 && top != 0 && none != top);
    uint8_t print_none[PIECE_PRINT_SIZE];
    memcpy(print_none, print_of(a), sizeof print_none);
    CHECK(memcmp(print_none, print_of(b), PIECE_PRINT_SIZE) != 0);   /* the tuning alone changes it */
}

/* dep59 step 3: what an added car's gallery reads of its piece (car_pieces_card): its card again, its skins/ folders in name order after
 * the skin as it comes, and which of them have masks (Color applies); 0 for an original or a number not registered */
static void test_the_card(void)
{
    char d[400], skins[500];
    CHECK(piece("carded", "card-car", "CARDCAR", "sound_from=viper\nyear=1992\npaints=Red:c8102e,Blue:1040c0\n", "", d));
    snprintf(skins, sizeof skins, "%s/skins/stripes", d);
    write_png(skins, "topv.png", 297, 105);   /* the Viper's topv */
    write_png(skins, "topv_mask.png", 297, 105);
    snprintf(skins, sizeof skins, "%s/skins/plain", d);
    write_png(skins, "topv.png", 297, 105);
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 1);
    CarPieceCard c;
    CHECK(car_pieces_card(16, &c));
    CHECK(strcmp(c.info.id, "card-car") == 0 && strcmp(c.info.year, "1992") == 0 && c.info.paint_count == 2);
    CHECK(strcmp(c.dir, d) == 0);
    CHECK_EQ_INT(c.skins, 3);
    CHECK(c.skin[0][0] == '\0' && strcmp(c.skin[1], "plain") == 0 && strcmp(c.skin[2], "stripes") == 0);
    CHECK(c.skin_masked[0] == 0 && c.skin_masked[1] == 0 && c.skin_masked[2] == 1);   /* no car_masks/CARDCAR/: the skin as it comes has none */
    snprintf(skins, sizeof skins, "%s/car_masks/CARDCAR", d);
    write_png(skins, "topv_mask.png", 297, 105);
    CHECK(car_pieces_card(16, &c));
    CHECK_EQ_INT(c.skin_masked[0], 1);
    CHECK(!car_pieces_card(3, &c) && !car_pieces_card(17, &c));
    write_text(d, "mod.ini", "id=other-car\nname=O\nversion=1\ntype=car\nstem=CARDCAR\nshort_name=T\n");   /* swapped since the load */
    CHECK(!car_pieces_card(16, &c) && c.info.id[0] == '\0');
    car_registry_clear();
}

/* dep59 step 3 (Task 4): a piece without its card, with a skin of a bad name or a texture of another size, is refused when the game
 * loads it, as at its installation (car_piece_check) */
static void test_pictures_at_load(void)
{
    char d[400], path[500];
    CHECK(piece("nocard", "no-card", "NOCARD", FROM, "", d));
    snprintf(path, sizeof path, "%s/card.png", d);
    unlink(path);
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 0);
    CHECK(piece("badskin", "bad-skin", "BADSKIN", FROM, "", d));
    snprintf(path, sizeof path, "%s/skins/Bad_Name", d);
    write_png(path, "topv.png", 297, 105);
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 0);
    CHECK(piece("wideskin", "wide-skin", "WIDESKIN", FROM, "", d));
    snprintf(path, sizeof path, "%s/skins/race", d);
    write_png(path, "topv.png", 296, 105);
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 0);
    write_png(path, "topv.png", 297, 105);   /* its size: it races */
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 1);
    snprintf(path, sizeof path, "%s/card.png", d);   /* a card that is no file (a folder; a device or a pipe alike): not read, refused */
    unlink(path);
    mkdir(path, 0777);
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 0);
    rmdir(path);
    car_registry_clear();
}

/* dep59 showcase (PR B): the photos= and photo_small= files are read with the piece: named and there it races, named and missing it is
 * refused (PIECE.PHOTO), as the pack and the studio say */
static void test_photos_at_load(void)
{
    char d[400], path[500];
    CHECK(piece("photos", "photo-car", "PHOTOCAR", "sound_from=viper\nphotos=big1.png,big2.png\nphoto_small=small.png\n", "", d));
    write_png(d, "big1.png", 450, 254);
    write_png(d, "big2.png", 450, 254);
    write_png(d, "small.png", 278, 228);
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 1);
    snprintf(path, sizeof path, "%s/big2.png", d);
    unlink(path);
    car_registry_clear();
    CHECK_EQ_INT(load1(d, 1), 0);   /* a photo named and not there */
    car_registry_clear();
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
    test_the_fingerprint();
    test_the_card();
    test_pictures_at_load();
    test_photos_at_load();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
