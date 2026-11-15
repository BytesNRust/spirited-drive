/* dep59: the rules of a car piece (formats/car_piece.h), one for the game and the studio: the eight words of the original race models,
 * the stems an added car may take, the paths of its three files. */
#include "formats/car_piece.h"
#include "formats/mod_ini.h"
#include "formats/pbs.h"
#include "formats/validate.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_words(void)
{
    static const char *const WORDS[] = { "supra", "diablo", "911", "zr1", "512tr", "viper", "nsx", "rx7" };
    for (int i = 0; i < 8; i++) {
        CHECK_EQ_INT(car_piece_model_of_word(WORDS[i]), i);
        CHECK(car_piece_word_of_model(i) != NULL && strcmp(car_piece_word_of_model(i), WORDS[i]) == 0);
    }
    CHECK_EQ_INT(car_piece_model_of_word("lada"), -1);
    CHECK_EQ_INT(car_piece_model_of_word("Viper"), -1);
    CHECK_EQ_INT(car_piece_model_of_word(""), -1);
    CHECK_EQ_INT(car_piece_model_of_word(NULL), -1);
    CHECK(car_piece_word_of_model(8) == NULL && car_piece_word_of_model(-1) == NULL);
}

static void test_stems(void)
{
    static const char *const OK[] = { "F40", "A1", "TESTCAR8", "M", "MX5" };
    static const char *const BAD[] = { "", "f40", "TOOLONG99", "A/B", "..", "A B", "DVIPER", "mrx7", "MRX7", "MTSUPRA", "TRAFFC", "COPMUST",
                                       "MCOPMUST", "P911T", "MP911T", "WAGON", "MJEEP", "TSUPRA", "LDIABL" };
    for (size_t i = 0; i < sizeof OK / sizeof OK[0]; i++) CHECK_EQ_INT(car_piece_stem_ok(OK[i]), 1);
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) CHECK_EQ_INT(car_piece_stem_ok(BAD[i]), 0);
    CHECK_EQ_INT(car_piece_stem_ok(NULL), 0);
}

static void test_paths(void)
{
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    car_piece_paths("F40", paths);
    CHECK(strcmp(paths[0], "SIMDATA/CARFAMS/F40.CFM") == 0);
    CHECK(strcmp(paths[1], "SIMDATA/CARFAMS/F40.PDN") == 0);
    CHECK(strcmp(paths[2], "SIMDATA/CARSPECS/F40.PBS") == 0);
}

/* ---- car_piece_check, on copies of the CD's Viper under the stem TESTCAR (skipped without the CD, as test_models) */

static uint8_t *read_cd(const char *rel, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/%s", TNFS_SOURCE_DIR, rel);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = (size_t)n;
    return d;
}

static ModInfo card(const char *text)
{
    ModInfo m;
    char why[160];
    if (!mod_ini_parse((const uint8_t *)text, strlen(text), &m, why, sizeof why)) fprintf(stderr, "card: %s\n", why);
    return m;
}

static const char GOOD[] = "id=test-car\nname=Test Car\nversion=1\ntype=car\nstem=TESTCAR\nshort_name=TEST\nsound_from=viper\nai_from=viper\n";

static const char *source = "DVIPER";   /* the CD car whose files the piece is made of */

/* the piece's three files, `source`'s (the Viper's), named `cfm`, `pdn`, `pbs` (NULL: left out); the PBS as its raw record, `edit` applied */
static int piece(FileSet *set, const char *cfm, const char *pdn, const char *pbs, void (*edit)(uint8_t *record))
{
    char CD[3][64];
    snprintf(CD[0], sizeof CD[0], "SIMDATA/CARFAMS/%s.CFM", source);
    snprintf(CD[1], sizeof CD[1], "SIMDATA/CARFAMS/%s.PDN", source);
    snprintf(CD[2], sizeof CD[2], "SIMDATA/CARSPECS/%s.PBS", source);
    const char *const names[3] = { cfm, pdn, pbs };
    fileset_init(set);
    for (int i = 0; i < 3; i++) {
        size_t size = 0;
        uint8_t *d = read_cd(CD[i], &size);
        if (d == NULL) return 0;
        if (i == 2) {
            uint8_t record[PBS_SIZE];
            CHECK(pbs_read(record, d, size));
            if (edit != NULL) edit(record);
            free(d);
            d = malloc(PBS_SIZE);
            memcpy(d, record, PBS_SIZE);
            size = PBS_SIZE;
        }
        if (names[i] == NULL || !fileset_add_owned(set, names[i], d, size)) free(d);
    }
    return 1;
}

static int has(const Report *r, const char *id, const char *text)
{
    for (size_t i = 0; i < r->count; i++)
        if (strcmp(r->item[i].limit->id, id) == 0 && (text == NULL || strstr(r->item[i].item, text) != NULL)) return 1;
    return 0;
}

/* car_piece_check of `text` and the files: its answer; the report in `r` (freed by the caller) */
static int check(const char *text, const char *cfm, const char *pdn, const char *pbs, void (*edit)(uint8_t *), Report *r)
{
    FileSet set;
    report_init(r);
    if (!piece(&set, cfm, pdn, pbs, edit)) return -1;
    const ModInfo m = card(text);
    const int ok = car_piece_check(&m, &set, r);
    fileset_free(&set);
    return ok;
}

static void le32_put(uint8_t *p, int32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static void no_force(uint8_t *rec) { le32_put(rec + CAR_PIECE_PBS_FORCE_SCALE, 0), pbs_seal(rec); }
static void no_320(uint8_t *rec) { le32_put(rec + CAR_PIECE_PBS_UNKNOWN_320, 0), pbs_seal(rec); }
static void no_power(uint8_t *rec) { le32_put(rec + CAR_PIECE_PBS_POWER, 0), pbs_seal(rec); }
static void many_points(uint8_t *rec) { le32_put(rec + CAR_PIECE_PBS_TORQUE_POINTS, 59), pbs_seal(rec); }   /* into shift_rpm */
static void most_points(uint8_t *rec) { le32_put(rec + CAR_PIECE_PBS_TORQUE_POINTS, 58), pbs_seal(rec); }
static void no_points(uint8_t *rec) { le32_put(rec + CAR_PIECE_PBS_TORQUE_POINTS, 0), pbs_seal(rec); }
static void minus_points(uint8_t *rec) { le32_put(rec + CAR_PIECE_PBS_TORQUE_POINTS, -1), pbs_seal(rec); }
static void unsealed(uint8_t *rec) { rec[0x100] ^= 0x5a; }

static const char *const CFM = "SIMDATA/CARFAMS/TESTCAR.CFM", *const PDN = "SIMDATA/CARFAMS/TESTCAR.PDN", *const PBS = "SIMDATA/CARSPECS/TESTCAR.PBS";

static void test_the_check(void)
{
    Report r;
    int ok = check(GOOD, CFM, PDN, PBS, NULL, &r);
    if (ok < 0) {
        fprintf(stderr, "car_piece: no CD in original/, the check's tests are skipped\n");
        report_free(&r);
        return;
    }
    CHECK_EQ_INT(ok, 1);   /* the Viper's own files: a good piece */
    CHECK_EQ_INT((int)report_fatal_count(&r), 0);
    report_free(&r);
    static const char *const CARS[8] = { "TSUPRA", "LDIABL", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7" };
    for (int i = 0; i < 8; i++) {   /* each of the CD's eight, as a piece: no finding at all */
        source = CARS[i];
        CHECK_EQ_INT(check(GOOD, CFM, PDN, PBS, NULL, &r), 1);
        if (r.count != 0) fprintf(stderr, "%s: %zu findings\n", CARS[i], r.count);
        CHECK_EQ_INT((int)r.count, 0);
        report_free(&r);
    }
    source = "DVIPER";
    CHECK_EQ_INT(check(GOOD, "simdata/carfams/testcar.cfm", "simdata/carfams/testcar.pdn", "simdata/carspecs/testcar.pbs", NULL, &r), 1);   /* any case */
    report_free(&r);
    const char *const missing[3][4] = { { NULL, "x", "x", "SIMDATA/CARFAMS/TESTCAR.CFM" }, { "x", NULL, "x", "SIMDATA/CARFAMS/TESTCAR.PDN" },
                                        { "x", "x", NULL, "SIMDATA/CARSPECS/TESTCAR.PBS" } };
    for (int i = 0; i < 3; i++) {
        CHECK_EQ_INT(check(GOOD, missing[i][0] ? CFM : NULL, missing[i][1] ? PDN : NULL, missing[i][2] ? PBS : NULL, NULL, &r), 0);
        CHECK(has(&r, "PIECE.FILE_MISSING", missing[i][3]));
        report_free(&r);
    }
    CHECK_EQ_INT(check(GOOD, CFM, PDN, "SIMDATA/CARSPECS/OTHER.PBS", NULL, &r), 0);   /* another stem's file is not this car's */
    CHECK(has(&r, "PIECE.FILE_MISSING", "TESTCAR.PBS"));
    report_free(&r);
    static const struct { const char *text, *id; } CARDS[] = {
        { "id=t\nname=T\nversion=1\ntype=tweak\nstem=TESTCAR\nsound_from=viper\nai_from=viper\n", "PIECE.TYPE" },
        { "id=t\nname=T\nversion=1\nstem=TESTCAR\nsound_from=viper\nai_from=viper\n", "PIECE.TYPE" },
        { "id=t\nname=T\nversion=1\ntype=car\nsound_from=viper\nai_from=viper\n", "PIECE.STEM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=DVIPER\nsound_from=viper\nai_from=viper\n", "PIECE.STEM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\n", "PIECE.FROM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nai_from=viper\n", "PIECE.FROM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=lada\nai_from=viper\n", "PIECE.FROM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nai_from=viper\ndash_from=lada\n", "PIECE.FROM" },
    };
    for (size_t i = 0; i < sizeof CARDS / sizeof CARDS[0]; i++) {
        CHECK_EQ_INT(check(CARDS[i].text, CFM, PDN, PBS, NULL, &r), 0);
        if (!has(&r, CARDS[i].id, NULL)) fprintf(stderr, "card %zu: no %s\n", i, CARDS[i].id);
        CHECK(has(&r, CARDS[i].id, NULL));
        report_free(&r);
    }
    CHECK_EQ_INT(check(GOOD, CFM, PDN, PBS, most_points, &r), 1);   /* the whole room */
    report_free(&r);
    CHECK_EQ_INT(check("id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=512tr\nai_from=diablo\ndash_from=nsx\n", CFM, PDN, PBS, NULL, &r), 1);
    report_free(&r);
    static const struct { void (*edit)(uint8_t *); const char *text; } SPECS[] = {
        { no_force, "force_scale" }, { no_320, "unknown_320" }, { no_power, "power" }, { many_points, "torque_points" },
        { no_points, "torque_points" }, { minus_points, "torque_points" }, { unsealed, "checksum" },
    };
    for (size_t i = 0; i < sizeof SPECS / sizeof SPECS[0]; i++) {
        CHECK_EQ_INT(check(GOOD, CFM, PDN, PBS, SPECS[i].edit, &r), 0);
        if (!has(&r, "PIECE.SPECS", SPECS[i].text)) fprintf(stderr, "specs %zu: no PIECE.SPECS naming %s\n", i, SPECS[i].text);
        CHECK(has(&r, "PIECE.SPECS", SPECS[i].text));
        report_free(&r);
    }
}

int main(void)
{
    test_words();
    test_stems();
    test_paths();
    test_the_check();
    return TEST_REPORT();
}
