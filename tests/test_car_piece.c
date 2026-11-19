/* dep59: the rules of a car piece (formats/car_piece.h), one for the game and the studio: the eight words of the original race models,
 * the stems an added car may take, the paths of its three files. The model's checks (tools stage A): the names the race looks up
 * (PIECE.MODEL_NAME and the race car's CFM.REQUIRED_NAME), the indexes the polygons hold (PIECE.MODEL_INDEX), the polygons no group
 * draws (PIECE.MODEL_GROUPS, a warning) and a PDN body that does not match the model (PIECE.BODY_SIZE, a warning). */
#include "formats/car_piece.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mod_ini.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "formats/png.h"
#include "formats/validate.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static const char GOOD[] = "id=test-car\nname=Test Car\nversion=1\ntype=car\nstem=TESTCAR\nshort_name=TEST\nsound_from=viper\n";

static const char *source = "DVIPER";   /* the CD car whose files the piece is made of */

/* dep59 step 3: the pictures every piece below brings besides its three files: card.png of 309 x 166 (unless `no_card`), then `extras` */
typedef struct { const char *name; int w, h; } Picture;
static const Picture *extras;
static size_t extra_count;
static int no_card;

/* a w x h grey PNG named `name` into `set` */
static void add_png(FileSet *set, const char *name, int w, int h)
{
    uint8_t *px = calloc((size_t)w * h, 1), pal[768] = { 0 }, *png = NULL;
    size_t len = 0;
    for (int i = 0; i < 768; i++) pal[i] = (uint8_t)(i / 3);
    if (px != NULL && png_write_indexed(px, (uint32_t)w, (uint32_t)h, pal, &png, &len) == PNG_OK) {
        uint8_t *own = malloc(len);
        memcpy(own, png, len);
        if (!fileset_add_owned(set, name, own, len)) free(own);
    }
    png_free(png), free(px);
}

static void add_pictures(FileSet *set)
{
    if (!no_card) add_png(set, "card.png", CAR_PIECE_CARD_W, CAR_PIECE_CARD_H);
    for (size_t i = 0; i < extra_count; i++) add_png(set, extras[i].name, extras[i].w, extras[i].h);
}

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
    add_pictures(set);
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
        { "id=t\nname=T\nversion=1\ntype=tweak\nstem=TESTCAR\nsound_from=viper\n", "PIECE.TYPE" },
        { "id=t\nname=T\nversion=1\nstem=TESTCAR\nsound_from=viper\n", "PIECE.TYPE" },
        { "id=t\nname=T\nversion=1\ntype=car\nsound_from=viper\n", "PIECE.STEM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=DVIPER\nsound_from=viper\n", "PIECE.STEM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\n", "PIECE.FROM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=lada\n", "PIECE.FROM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\ndash_from=lada\n", "PIECE.FROM" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nai_from=viper\n", "PIECE.FROM" },   /* 3b: gone */
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nai_level=turbo\n", "PIECE.AI" },
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nabs=maybe\n", "PIECE.ASSIST" },   /* dep78 */
        { "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\ntraction=1\n", "PIECE.ASSIST" },
    };
    for (size_t i = 0; i < sizeof CARDS / sizeof CARDS[0]; i++) {
        CHECK_EQ_INT(check(CARDS[i].text, CFM, PDN, PBS, NULL, &r), 0);
        if (!has(&r, CARDS[i].id, NULL)) fprintf(stderr, "card %zu: no %s\n", i, CARDS[i].id);
        CHECK(has(&r, CARDS[i].id, NULL));
        report_free(&r);
    }
    /* dep78: abs= and traction= are yes or no, the message names the key and the value; either, both or neither is a good card */
    CHECK_EQ_INT(check("id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nabs=maybe\n", CFM, PDN, PBS, NULL, &r), 0);
    CHECK(has(&r, "PIECE.ASSIST", "abs=maybe is not yes or no"));
    report_free(&r);
    CHECK_EQ_INT(check("id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\ntraction=on\n", CFM, PDN, PBS, NULL, &r), 0);
    CHECK(has(&r, "PIECE.ASSIST", "traction=on is not yes or no"));
    report_free(&r);
    CHECK_EQ_INT(check("id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nabs=yes\ntraction=no\n", CFM, PDN, PBS, NULL, &r), 1);
    report_free(&r);
    {   /* the resolver: the key when given, else the PBS word (little endian, positive) at 0x334 / 0x338 */
        uint8_t pbs[PBS_SIZE] = { 0 };   /* a raw record */
        pbs[CAR_PIECE_PBS_ABS] = 1;   /* ABS in the PBS, traction not */
        CHECK_EQ_INT(car_piece_assist("", pbs, sizeof pbs, CAR_PIECE_PBS_ABS), 1);
        CHECK_EQ_INT(car_piece_assist("", pbs, sizeof pbs, CAR_PIECE_PBS_TRACTION), 0);
        CHECK_EQ_INT(car_piece_assist("no", pbs, sizeof pbs, CAR_PIECE_PBS_ABS), 0);        /* the key wins over the PBS ... */
        CHECK_EQ_INT(car_piece_assist("yes", pbs, sizeof pbs, CAR_PIECE_PBS_TRACTION), 1);  /* ... both ways */
        pbs[CAR_PIECE_PBS_TRACTION + 3] = 0x80;   /* a negative word is not positive */
        CHECK_EQ_INT(car_piece_assist("", pbs, sizeof pbs, CAR_PIECE_PBS_TRACTION), 0);
        CHECK_EQ_INT(car_piece_assist("", pbs, 0x336, CAR_PIECE_PBS_ABS), 0);               /* not a PBS: a file too short */
        CHECK_EQ_INT(car_piece_assist("yes", NULL, 0, CAR_PIECE_PBS_ABS), 1);
    }
    /* 3b: the reasons say what to write instead (Review Focus 2, 3); no ai_level, or one of the four, is a good card */
    CHECK_EQ_INT(check("id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nai_from=viper\n", CFM, PDN, PBS, NULL, &r), 0);
    CHECK(has(&r, "PIECE.FROM", "ai_from is gone: a car's AI is its own, ai_level= picks one of slow, medium, fast or top"));
    report_free(&r);
    CHECK_EQ_INT(check("id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\nai_level=turbo\n", CFM, PDN, PBS, NULL, &r), 0);
    CHECK(has(&r, "PIECE.AI", "ai_level=turbo is not slow, medium, fast or top"));
    report_free(&r);
    static const char *const LEVELS[] = { "", "ai_level=slow\n", "ai_level=medium\n", "ai_level=fast\n", "ai_level=top\n" };
    for (size_t i = 0; i < sizeof LEVELS / sizeof LEVELS[0]; i++) {
        char card[200];
        snprintf(card, sizeof card, "id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\n%s", LEVELS[i]);
        CHECK_EQ_INT(check(card, CFM, PDN, PBS, NULL, &r), 1);
        report_free(&r);
    }
    CHECK_EQ_INT(check(GOOD, CFM, PDN, PBS, most_points, &r), 1);   /* the whole room */
    report_free(&r);
    CHECK_EQ_INT(check("id=t\nname=T\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=512tr\ndash_from=nsx\n", CFM, PDN, PBS, NULL, &r), 1);
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

/* ---- the model: the Viper's CFM and PDN edited in place */

typedef enum { TABLE_A, TABLE_B, TABLE_C, TABLE_LABEL } Table;

/* the high-detail mesh of the CFM in `cfm` (raw, as the CD's) */
static int high_mesh(uint8_t *cfm, size_t size, Mesh *mesh)
{
    Chunk root, high;
    return chunk_open(&root, cfm, size) == CHUNK_OK && chunk_child(&root, CFM_CHILD_MESH_HIGH, &high) == CHUNK_OK &&
           mesh_open(mesh, &high) == MESH_OK;
}

/* the record of `table` named `name` (any case) gets the name `to`; 1 when one was found */
static int rename_record(uint8_t *cfm, size_t size, Table table, const char *name, const char *to)
{
    Mesh m;
    if (!high_mesh(cfm, size, &m)) return 0;
    static const uint32_t SIZE[] = { 20, 28, 12, 12 };
    const uint32_t offset[] = { m.section_a_offset, m.section_b_offset, m.section_c_offset, m.label_offset };
    const uint32_t count[] = { m.section_a_count, m.section_b_count, m.section_c_count, m.label_count };
    for (uint32_t i = 0; i < count[table]; i++) {
        uint8_t *p = (uint8_t *)m.base + offset[table] + (size_t)i * SIZE[table];
        char stored[9] = { 0 };
        memcpy(stored, p, 8);
        if (strcasecmp(stored, name) != 0) continue;
        memset(p, 0, 8);
        memcpy(p, to, strlen(to) < 8 ? strlen(to) : 8);
        return 1;
    }
    return 0;
}

static const char *model_edit_name;

static void no_smoke(uint8_t *cfm, size_t n) { model_edit_name = rename_record(cfm, n, TABLE_C, "smok", "smog") ? "smok" : NULL; }
static void no_light(uint8_t *cfm, size_t n) { model_edit_name = rename_record(cfm, n, TABLE_LABEL, "lt_frnt", "lt_frnx") ? "lt_frnt" : NULL; }
static void no_point(uint8_t *cfm, size_t n) { model_edit_name = rename_record(cfm, n, TABLE_C, "fl0", "fl9") ? "fl0" : NULL; }
static void no_turn(uint8_t *cfm, size_t n) { model_edit_name = rename_record(cfm, n, TABLE_A, "left_tur", "left_tux") ? "left_turn" : NULL; }
static void no_inside(uint8_t *cfm, size_t n) { model_edit_name = rename_record(cfm, n, TABLE_B, "inside", "insidx") ? "inside" : NULL; }
/* polygon 0's texture past the table */
static void bad_texture(uint8_t *cfm, size_t n)
{
    Mesh m;
    model_edit_name = NULL;
    if (!high_mesh(cfm, n, &m) || m.texture_count >= 0xff) return;
    ((uint8_t *)m.base + m.polygon_offset)[2] = 0xff;
    model_edit_name = "texture";
}
/* polygon 0's first corner past the vertex table */
static void bad_vertex(uint8_t *cfm, size_t n)
{
    Mesh m;
    MeshPolygon p;
    model_edit_name = NULL;
    if (!high_mesh(cfm, n, &m) || mesh_polygon(&m, 0, &p) != MESH_OK || p.vertex_slot >= m.vertex_map_slots) return;
    uint8_t *slot = (uint8_t *)m.base + m.vertex_map_offset + (size_t)p.vertex_slot * 4;
    slot[0] = slot[1] = slot[2] = 0xff, slot[3] = 0;
    model_edit_name = "vertex";
}
/* the groups the race can do without (outside, convex) unnamed: what only they drew is in no group */
static void lone_inside(uint8_t *cfm, size_t n)
{
    const int b = rename_record(cfm, n, TABLE_B, "outside", "outsidx"), c = rename_record(cfm, n, TABLE_B, "convex", "convexx");
    model_edit_name = b || c ? "group" : NULL;
}

/* car_piece_check of the good card on the Viper's files, its CFM given to `cfm_edit` and its PDN's first word multiplied by `pdn_scale` */
static int check_model(void (*cfm_edit)(uint8_t *, size_t), int pdn_scale, Report *r)
{
    FileSet set;
    report_init(r);
    source = "DVIPER";
    if (!piece(&set, CFM, PDN, PBS, NULL)) return -1;
    SetFile *cfm = (SetFile *)fileset_find(&set, CFM), *pdn = (SetFile *)fileset_find(&set, PDN);
    if (cfm_edit != NULL) cfm_edit(cfm->data, cfm->size);
    if (pdn_scale != 1) {
        uint8_t rec[PDN_SIZE];
        size_t len = 0;
        static uint8_t packed[4096];
        CHECK(pdn_read(rec, pdn->data, pdn->size));
        const int32_t x = pdn_i32(rec, 0) * pdn_scale;
        rec[0] = (uint8_t)x, rec[1] = (uint8_t)(x >> 8), rec[2] = (uint8_t)(x >> 16), rec[3] = (uint8_t)(x >> 24);
        CHECK(pdn_pack(rec, packed, sizeof packed, &len) == QFS_OK);
        free(pdn->data);
        pdn->data = malloc(len), pdn->size = len;
        memcpy(pdn->data, packed, len);
    }
    const ModInfo m = card(GOOD);
    const int ok = car_piece_check(&m, &set, r);
    fileset_free(&set);
    return ok;
}

static void test_the_model(void)
{
    Report r;
    if (check_model(NULL, 1, &r) < 0) {
        report_free(&r);
        return;
    }
    CHECK_EQ_INT((int)r.count, 0);
    report_free(&r);
    static const struct { void (*edit)(uint8_t *, size_t); const char *id; } FATAL[] = {
        { no_smoke, "PIECE.MODEL_NAME" }, { no_light, "PIECE.MODEL_NAME" }, { no_point, "PIECE.MODEL_NAME" },
        { no_turn, "CFM.REQUIRED_NAME" }, { no_inside, "CFM.REQUIRED_NAME" },
        { bad_texture, "PIECE.MODEL_INDEX" }, { bad_vertex, "PIECE.MODEL_INDEX" },
    };
    for (size_t i = 0; i < sizeof FATAL / sizeof FATAL[0]; i++) {
        const int ok = check_model(FATAL[i].edit, 1, &r);
        CHECK(model_edit_name != NULL);   /* the Viper has what the edit changes */
        CHECK_EQ_INT(ok, 0);
        if (model_edit_name != NULL && !has(&r, FATAL[i].id, model_edit_name))
            fprintf(stderr, "model %zu: no %s naming %s\n", i, FATAL[i].id, model_edit_name);
        CHECK(model_edit_name != NULL && has(&r, FATAL[i].id, model_edit_name));
        report_free(&r);
    }
    CHECK_EQ_INT(check_model(lone_inside, 1, &r), 1);   /* a warning: those polygons are not drawn close up */
    CHECK(model_edit_name != NULL && has(&r, "PIECE.MODEL_GROUPS", NULL));
    report_free(&r);
    CHECK_EQ_INT(check_model(NULL, 2, &r), 1);   /* the body twice as wide as the model: a warning */
    CHECK(has(&r, "PIECE.BODY_SIZE", PDN));
    report_free(&r);
}

/* dep59 step 3: card.png there and of its size (Review Focus 4), rival.png and gallery.png of theirs, the skins' names, count and files
 * against the CFM (Review Focus 5) */
static int check_pictures(const Picture *p, size_t n, Report *r)
{
    extras = p, extra_count = n;
    const int ok = check(GOOD, CFM, PDN, PBS, NULL, r);
    extras = NULL, extra_count = 0;
    return ok;
}

/* the check of `text`'s card with `p` (n pictures) added; the report in `r` */
static int check_photos(const char *text, const Picture *p, size_t n, Report *r)
{
    extras = p, extra_count = n;
    const int ok = check(text, CFM, PDN, PBS, NULL, r);
    extras = NULL, extra_count = 0;
    return ok;
}

/* port (dep59 showcase): the photos= and photo_small= files are there, readable PNGs from PHOTO_MIN to PHOTO_MAX a side, and no PNG of the
 * piece's root is left out of them */
static void test_the_photos(void)
{
    static const char TEXT[] = "id=test-car\nname=Test Car\nversion=1\ntype=car\nstem=TESTCAR\nshort_name=TEST\nsound_from=viper\n"
                               "photos=front.png,side.png\nphoto_small=eng.png\n";
    Report r;
    const Picture good[] = { { "front.png", 450, 254 }, { "side.png", 320, 180 }, { "eng.png", 278, 228 } };
    const int cd = check_photos(TEXT, good, 3, &r);
    CHECK(cd != 0 || r.count == 0);
    const int have_cd = cd >= 0;
    report_free(&r);
    if (!have_cd) return;
    CHECK_EQ_INT(check_photos(TEXT, good, 3, &r), 1);
    CHECK_EQ_INT((int)r.count, 0);
    report_free(&r);
    const Picture no_side[] = { { "front.png", 450, 254 }, { "eng.png", 278, 228 } };
    CHECK_EQ_INT(check_photos(TEXT, no_side, 2, &r), 0);
    CHECK(has(&r, "PIECE.PHOTO", "side.png"));
    report_free(&r);
    const Picture small[] = { { "front.png", 50, 30 }, { "side.png", 320, 180 }, { "eng.png", 278, 228 } };
    CHECK_EQ_INT(check_photos(TEXT, small, 3, &r), 0);
    CHECK(has(&r, "PIECE.PHOTO", "front.png") && has(&r, "PIECE.PHOTO", "50x30"));
    report_free(&r);
    const Picture huge[] = { { "front.png", 450, 254 }, { "side.png", 320, 180 }, { "eng.png", 2000, 1500 } };
    CHECK_EQ_INT(check_photos(TEXT, huge, 3, &r), 0);
    CHECK(has(&r, "PIECE.PHOTO", "eng.png") && has(&r, "PIECE.PHOTO", "2000x1500"));
    report_free(&r);
    const Picture extra[] = { { "front.png", 450, 254 }, { "side.png", 320, 180 }, { "eng.png", 278, 228 }, { "forgotten.png", 300, 200 } };
    CHECK_EQ_INT(check_photos(TEXT, extra, 4, &r), 0);   /* a PNG in the root that no key names */
    CHECK(has(&r, "PIECE.PHOTO", "forgotten.png") && has(&r, "PIECE.PHOTO", "photos="));
    report_free(&r);
    CHECK_EQ_INT(check_photos(GOOD, NULL, 0, &r), 1);   /* none given: nothing asked */
    report_free(&r);
    CHECK(car_piece_picture("front.png") && car_piece_picture("a-b_c.1.png"));   /* a root PNG of a photo's name is one of the piece's pictures */
    CHECK(!car_piece_picture("Front.png") && !car_piece_picture("front.PNG") && !car_piece_picture("dir/front.png") && !car_piece_picture("front.jpg"));
}

static void test_the_pictures(void)
{
    Report r;
    const int cd = check_pictures(NULL, 0, &r);
    report_free(&r);
    if (cd < 0) return;
    no_card = 1;
    CHECK_EQ_INT(check_pictures(NULL, 0, &r), 0);
    CHECK(has(&r, "PIECE.CARD", "card.png"));
    report_free(&r);
    const Picture wide_card[] = { { "card.png", 310, 166 } };
    CHECK_EQ_INT(check_pictures(wide_card, 1, &r), 0);
    CHECK(has(&r, "PIECE.CARD", "310x166"));
    report_free(&r);
    no_card = 0;
    const Picture good[] = { { "rival.png", 284, 117 }, { "gallery.png", 320, 200 }, { "skins/race/topv.png", 297, 105 },
                             { "skins/race/topv_mask.png", 297, 105 }, { "skins/race-2/topv_mask.png", 50, 20 } };
    CHECK_EQ_INT(check_pictures(good, 5, &r), 1);
    CHECK_EQ_INT((int)r.count, 0);
    report_free(&r);
    static const struct { Picture p; const char *id, *a, *b; } BAD[] = {
        { { "rival.png", 284, 118 }, "PIECE.PICTURE", "rival.png", "284x118" },
        { { "gallery.png", 640, 400 }, "PIECE.PICTURE", "gallery.png", "640x400" },
        { { "skins/race/zzzz.png", 297, 105 }, "PIECE.SKIN", "race", "zzzz" },
        { { "skins/race/topv.png", 296, 105 }, "PIECE.SKIN", "race", "296x105" },
        { { "skins/race/zzzz_lo.png", 10, 10 }, "PIECE.SKIN", "low-detail", "zzzz" },
        { { "skins/race/topv_hi.png", 297, 105 }, "PIECE.SKIN", "topv_hi", NULL },
        { { "skins/Race/topv.png", 297, 105 }, "PIECE.SKIN", "Race", NULL },
        { { "skins/race/topv_mask.png", 3000, 1 }, "PIECE.SKIN", "mask", NULL },
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        CHECK_EQ_INT(check_pictures(&BAD[i].p, 1, &r), 0);
        const int named = has(&r, BAD[i].id, BAD[i].a) && (BAD[i].b == NULL || has(&r, BAD[i].id, BAD[i].b));
        if (!named) fprintf(stderr, "picture %zu: no %s naming %s\n", i, BAD[i].id, BAD[i].p.name);
        CHECK(named);
        report_free(&r);
    }
    Picture skins[16];
    static char names[16][32];
    for (int i = 0; i < 16; i++) snprintf(names[i], sizeof names[i], "skins/s%d/topv.png", i), skins[i] = (Picture){ names[i], 297, 105 };
    CHECK_EQ_INT(check_pictures(skins, 15, &r), 1);   /* fifteen: the most */
    report_free(&r);
    CHECK_EQ_INT(check_pictures(skins, 16, &r), 0);
    CHECK(has(&r, "PIECE.SKIN", "15"));
    report_free(&r);
    static Picture many[CAR_PIECE_SKIN_FILES_MAX + 1];   /* a skin's files: up to 256 */
    static char many_names[CAR_PIECE_SKIN_FILES_MAX + 1][32];
    for (int i = 0; i <= CAR_PIECE_SKIN_FILES_MAX; i++)
        snprintf(many_names[i], sizeof many_names[i], "skins/race/t%03d.png", i), many[i] = (Picture){ many_names[i], 1, 1 };
    CHECK_EQ_INT(check_pictures(many, CAR_PIECE_SKIN_FILES_MAX + 1, &r), 0);
    CHECK(has(&r, "PIECE.SKIN", "more than 256 files"));
    report_free(&r);
    CHECK(car_piece_picture("card.png") && car_piece_picture("rival.png") && car_piece_picture("gallery.png"));
    CHECK(!car_piece_picture("CARD.PNG") && !car_piece_picture("SKINS/race/topv.png") && !car_piece_picture("skins/race/topv.PNG"));
    CHECK(car_piece_picture("skins/race/topv.png") && car_piece_picture("skins/Bad/x.png"));
    CHECK(!car_piece_picture("skins//x.png") && !car_piece_picture("skins/race/"));
    CHECK(!car_piece_picture("skins/race/readme.txt") && !car_piece_picture("skins/topv.png") && !car_piece_picture("skins/a/b/c.png"));
    CHECK(!car_piece_picture("preview.png") && !car_piece_picture("car_masks/TESTCAR/topv_mask.png"));
}

int main(void)
{
    test_words();
    test_stems();
    test_paths();
    test_the_check();
    test_the_model();
    test_the_pictures();
    test_the_photos();
    return TEST_REPORT();
}
