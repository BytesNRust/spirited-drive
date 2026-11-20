/* The track art file built from pictures (formats/fam_build.h): the tree the chunk reader walks, the banks the SHPI reader opens, the
 * variant sizes, no transparent pixel from opaque pictures and see-through ones from a picture with gaps (a fence's), the validator's limits, and the refusals. With the path to the CD's AL1 track file as an
 * argument, the terrain's variants have the CD's sizes. */
#include "formats/chunk.h"
#include "formats/fam_build.h"
#include "formats/track_template.h"
#include "formats/fsh.h"
#include "formats/validate.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SIDE = 32, TEXTURES = 2, SCENERY_SIDE = 4, HORIZON_W = 64, HORIZON_H = 32, MANY = 49, LIMIT_TERRAIN = 48, LIMIT_SCENERY = 64 };

static void fill_gradient(uint8_t *rgba, uint32_t w, uint32_t h, int seed)
{
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++) {
            uint8_t *p = rgba + 4 * ((size_t)y * w + x);
            p[0] = (uint8_t)(x * 255 / w + (uint32_t)seed), p[1] = (uint8_t)(y * 255 / h), p[2] = (uint8_t)(seed * 40), p[3] = 255;
        }
}

/* the pictures a test stage is made of; every picture is owned here */
typedef struct {
    uint8_t terrain[TEXTURES][SIDE * SIDE * 4];
    uint8_t scenery[SCENERY_SIDE * SCENERY_SIDE * 4];
    uint8_t horizon[HORIZON_W * HORIZON_H * 4];
    FamPicture terrain_pic[TEXTURES], scenery_pic[LIMIT_SCENERY], horizon_pic;
} Stage;

static void stage_init(Stage *s)
{
    for (int t = 0; t < TEXTURES; t++) {
        fill_gradient(s->terrain[t], SIDE, SIDE, t);
        s->terrain_pic[t] = (FamPicture){ SIDE, SIDE, s->terrain[t] };
    }
    fill_gradient(s->scenery, SCENERY_SIDE, SCENERY_SIDE, 3);
    for (int i = 0; i < LIMIT_SCENERY; i++) s->scenery_pic[i] = (FamPicture){ SCENERY_SIDE, SCENERY_SIDE, s->scenery };
    fill_gradient(s->horizon, HORIZON_W, HORIZON_H, 5);
    s->horizon_pic = (FamPicture){ HORIZON_W, HORIZON_H, s->horizon };
}

static int build(const FamSource *src, uint8_t **out, size_t *len, char *msg, size_t cap)
{
    ExchangeError err = { { 0 } };
    const int ok = fam_build(src, out, len, &err);
    snprintf(msg, cap, "%s", err.message);
    return ok;
}

static int bank_open(const Chunk *chunk, Fsh *fsh)
{
    return chunk->kind == CHUNK_KIND_BITMAP && chunk_bitmaps(chunk, fsh) == FSH_OK;
}

static int find_entry(const Fsh *fsh, const char *name, FshEntry *e)
{
    uint32_t index = 0;
    return fsh_find(fsh, name, &index) == FSH_OK && fsh_entry(fsh, index, e) == FSH_OK;
}

static void test_tree_and_banks(const uint8_t *file, size_t len)
{
    Chunk root, terrain, scenery, horizon, objects, bank0, bank1;
    ChunkTotals totals;
    CHECK(chunk_open(&root, file, len) == CHUNK_OK);
    CHECK(chunk_survey(&root, &totals) == CHUNK_OK);
    CHECK_EQ_INT(root.count, 4);
    CHECK(chunk_child(&root, FAM_CHILD_TERRAIN, &terrain) == CHUNK_OK);
    CHECK_EQ_INT(terrain.count, TEXTURES);
    CHECK(chunk_child(&terrain, 0, &bank0) == CHUNK_OK && chunk_child(&terrain, 1, &bank1) == CHUNK_OK);
    Fsh fsh;
    FshEntry e;
    CHECK(bank_open(&bank0, &fsh));
    CHECK(find_entry(&fsh, "!pal", &e) && e.type == 0x22);
    CHECK(find_entry(&fsh, "A000", &e) && e.width == 128 && e.height == 256);
    CHECK(find_entry(&fsh, "C007", &e) && e.width == 4 && e.height == 12);
    CHECK(bank_open(&bank1, &fsh) && find_entry(&fsh, "B003", &e) && e.width == 16 && e.height == 32);
    CHECK(chunk_child(&root, FAM_CHILD_SCENERY, &scenery) == CHUNK_OK);
    CHECK_EQ_INT(scenery.count, 1);
    CHECK(chunk_child(&root, FAM_CHILD_HORIZON, &horizon) == CHUNK_OK && bank_open(&horizon, &fsh));
    CHECK(find_entry(&fsh, "horz", &e) && e.width == HORIZON_W && e.height == HORIZON_H);
    CHECK(chunk_child(&root, FAM_CHILD_OBJECTS, &objects) == CHUNK_OK);
    CHECK_EQ_INT(objects.kind, CHUNK_KIND_HEADER);
    CHECK_EQ_INT(objects.count, 0);
}

/* the transparent index is never a pixel: every bitmap of every terrain and scenery bank, scanned */
static void test_no_transparent_pixels(const uint8_t *file, size_t len)
{
    Chunk root;
    CHECK(chunk_open(&root, file, len) == CHUNK_OK);
    int bad = 0;
    const uint32_t groups[2] = { FAM_CHILD_TERRAIN, FAM_CHILD_SCENERY };
    for (int g = 0; g < 2; g++) {
        Chunk group;
        chunk_child(&root, groups[g], &group);
        for (uint32_t b = 0; b < group.count; b++) {
            Chunk bank;
            Fsh fsh;
            if (chunk_child(&group, b, &bank) != CHUNK_OK || !bank_open(&bank, &fsh)) { bad++; continue; }
            for (uint32_t i = 0; i < fsh.count; i++) {
                FshEntry e;
                if (fsh_entry(&fsh, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP) continue;
                bad += memchr(e.payload, 0xff, (size_t)e.width * e.height) != NULL;
            }
        }
    }
    CHECK_EQ_INT(bad, 0);
}

static void test_validator_accepts(const uint8_t *file, size_t len)
{
    Report r;
    report_init(&r);
    validate_file(file, len, "stage.fam", &r);
    CHECK_EQ_INT(report_fatal_count(&r), 0);
    report_free(&r);
}

static void test_same_input_same_bytes(const FamSource *src, const uint8_t *file, size_t len)
{
    uint8_t *again = NULL;
    size_t again_len = 0;
    char msg[256];
    CHECK(build(src, &again, &again_len, msg, sizeof msg));
    CHECK_EQ_INT(again_len, len);
    CHECK(again_len == len && memcmp(again, file, len) == 0);
    free(again);
}

/* one refused build: the counts it is given and the words its message must hold */
typedef struct {
    size_t terrain, scenery;
    int horizon;
    const char *want;
} Refusal;

static void test_refusals(const Stage *s)
{
    static FamPicture many[MANY];
    for (int i = 0; i < MANY; i++) many[i] = s->terrain_pic[i % TEXTURES];
    static const Refusal cases[] = {
        { 1, 0, 1, "at least 2" },
        { 0, 0, 1, "at least 2" },
        { MANY, 0, 1, "at most 48" },
        { TEXTURES, LIMIT_SCENERY + 1, 1, "at most 63" },
        { LIMIT_TERRAIN, LIMIT_SCENERY - 15, 1, "at most 96" },
        { TEXTURES, 0, 0, "a stage needs a horizon" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const FamSource src = { many, cases[i].terrain, s->scenery_pic, cases[i].scenery, cases[i].horizon ? &s->horizon_pic : NULL, NULL, NULL };
        uint8_t *out = NULL;
        size_t len = 0;
        char msg[256];
        CHECK(!build(&src, &out, &len, msg, sizeof msg));
        CHECK(strstr(msg, cases[i].want) != NULL);
        if (strstr(msg, cases[i].want) == NULL) fprintf(stderr, "refusal %zu got: %s\n", i, msg);
        free(out);
    }
}

/* 48 terrain textures are inside the count, but their banks are 6 MB: the file limit refuses them */
static void test_file_limit(const Stage *s)
{
    static FamPicture many[LIMIT_TERRAIN];
    for (int i = 0; i < LIMIT_TERRAIN; i++) many[i] = s->terrain_pic[i % TEXTURES];
    const FamSource src = { many, LIMIT_TERRAIN, NULL, 0, &s->horizon_pic, NULL, NULL };
    uint8_t *out = NULL;
    size_t len = 0;
    char msg[256];
    CHECK(!build(&src, &out, &len, msg, sizeof msg));
    CHECK(strstr(msg, "the FAM would be") != NULL);
    free(out);
}

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *len = n > 0 ? (size_t)n : 0;
    return d;
}

/* the terrain's variants have the sizes AL1's terrain has (the ladder in fam_build.c) */
static void test_ladder_matches_cd(const uint8_t *built, size_t built_len, const char *cd_path)
{
    size_t cd_len = 0;
    uint8_t *cd = read_file(cd_path, &cd_len);
    CHECK(cd != NULL);
    if (cd == NULL) return;
    Chunk cd_root, cd_terrain, cd_bank, our_root, our_terrain, our_bank;
    CHECK(chunk_open(&cd_root, cd, cd_len) == CHUNK_OK && chunk_child(&cd_root, FAM_CHILD_TERRAIN, &cd_terrain) == CHUNK_OK);
    CHECK(chunk_child(&cd_terrain, 0, &cd_bank) == CHUNK_OK);
    CHECK(chunk_open(&our_root, built, built_len) == CHUNK_OK && chunk_child(&our_root, FAM_CHILD_TERRAIN, &our_terrain) == CHUNK_OK);
    CHECK(chunk_child(&our_terrain, 0, &our_bank) == CHUNK_OK);
    Fsh cd_fsh, our_fsh;
    CHECK(bank_open(&cd_bank, &cd_fsh) && bank_open(&our_bank, &our_fsh));
    for (int v = 0; v < FAM_VARIANTS; v++) {
        char name[5];
        snprintf(name, sizeof name, "A00%d", v);
        FshEntry a, b;
        CHECK(find_entry(&cd_fsh, name, &a) && find_entry(&our_fsh, name, &b));
        CHECK_EQ_INT(b.width, a.width);
        CHECK_EQ_INT(b.height, a.height);
    }
    free(cd);
}

/* a bank holds the letters it is asked for and no others (the palette always), and the file is smaller for it */
static int letter_count(const Fsh *fsh, char letter)
{
    int n = 0;
    for (int v = 0; v < FAM_VARIANTS; v++) {
        char name[5];
        snprintf(name, sizeof name, "%c00%d", letter, v);
        FshEntry e;
        n += find_entry(fsh, name, &e);
    }
    return n;
}

static void test_letters(const Stage *stage, size_t all_len)
{
    unsigned letters[TEXTURES];
    for (int i = 0; i < TEXTURES; i++) letters[i] = FAM_LETTERS_ALL;
    letters[0] = FAM_LETTER_A;
    letters[1] = FAM_LETTER_B | FAM_LETTER_C;
    const FamSource src = { stage->terrain_pic, TEXTURES, stage->scenery_pic, 1, &stage->horizon_pic, letters, NULL };
    uint8_t *file = NULL;
    size_t len = 0;
    char msg[256];
    CHECK(build(&src, &file, &len, msg, sizeof msg));
    if (file == NULL) return;
    CHECK(len < all_len);
    Chunk root, terrain, bank;
    CHECK(chunk_open(&root, file, len) == CHUNK_OK && chunk_child(&root, FAM_CHILD_TERRAIN, &terrain) == CHUNK_OK);
    const int want[2][3] = { { 8, 0, 0 }, { 0, 8, 8 } };
    for (int b = 0; b < TEXTURES; b++) {
        Fsh fsh;
        FshEntry pal;
        CHECK(chunk_child(&terrain, (uint32_t)b, &bank) == CHUNK_OK && bank_open(&bank, &fsh));
        CHECK(find_entry(&fsh, "!pal", &pal));   /* every bank keeps the stage's palette */
        for (int l = 0; l < 3; l++) CHECK_EQ_INT(letter_count(&fsh, (char)('A' + l)), want[b][l]);
    }
    free(file);
}

/* the refusal says what the check wants (the letters of the banks) and where the guide explains it, not a path outside the repository */
static int expects_and_points(const char *m)
{
    return strstr(m, "docs/modding.md") != NULL && strstr(m, "Art for a generic track") != NULL && strstr(m, "art/tnfs-tracks") == NULL &&
           strstr(m, "letter A") != NULL && strstr(m, "letter C") != NULL;
}

/* the generic road's check of a FAM: 11 banks with the letter C in the last; fewer banks, or a last bank without C, are refused; letters of 0 are refused */
static void test_generic_check(const Stage *s)
{
    static FamPicture many[TRACK_GENERIC_BANKS];
    static unsigned letters[TRACK_GENERIC_BANKS];
    for (int i = 0; i < TRACK_GENERIC_BANKS; i++) many[i] = s->terrain_pic[i % TEXTURES], letters[i] = FAM_LETTER_A;
    letters[TRACK_GENERIC_BANKS - 1] = FAM_LETTER_C;
    const FamSource good = { many, TRACK_GENERIC_BANKS, NULL, 0, &s->horizon_pic, letters, NULL };
    const FamSource few = { many, TRACK_GENERIC_BANKS - 1, NULL, 0, &s->horizon_pic, letters, NULL };
    uint8_t *file = NULL;
    size_t len = 0;
    char msg[256];
    ExchangeError err = { { 0 } };
    CHECK(build(&good, &file, &len, msg, sizeof msg));
    if (file != NULL) CHECK(track_template_check_fam(file, len, &err));
    free(file);
    file = NULL;
    CHECK(build(&few, &file, &len, msg, sizeof msg));
    if (file != NULL) {
        CHECK(!track_template_check_fam(file, len, &err));
        CHECK(strstr(err.message, "terrain banks") != NULL);
        CHECK(expects_and_points(err.message));
        char want[16];
        snprintf(want, sizeof want, "%d needed", (int)TRACK_GENERIC_BANKS);   /* the number the check uses, not a copy of it */
        CHECK(strstr(err.message, want) != NULL);
    }
    free(file);
    file = NULL;
    letters[TRACK_GENERIC_BANKS - 1] = FAM_LETTER_A;   /* the fence's bank has no C */
    CHECK(build(&good, &file, &len, msg, sizeof msg));
    if (file != NULL) {
        CHECK(!track_template_check_fam(file, len, &err));
        CHECK(strstr(err.message, "letter C") != NULL);
        CHECK(strstr(err.message, "docs/modding.md") != NULL && strstr(err.message, "Art for a generic track") != NULL);
    }
    free(file);
    file = NULL;
    CHECK(!track_template_check_fam((const uint8_t *)"junk", 4, &err));
    letters[0] = 0;
    CHECK(!build(&good, &file, &len, msg, sizeof msg));
    CHECK(strstr(msg, "letters") != NULL);
}

/* a fence with gaps (step: fences): terrain 1's left half is see-through (alpha 0). Every bitmap of its bank, at every variant size, holds
 * the transparent index 0xFF (the game draws it see-through, track_art.c) and seen pixels too; bank 0's, opaque, holds none */
static void test_see_through_bank(const Stage *stage)
{
    static uint8_t gaps[SIDE * SIDE * 4];
    memcpy(gaps, stage->terrain[1], sizeof gaps);
    for (int y = 0; y < SIDE; y++)
        for (int x = 0; x < SIDE / 2; x++) gaps[4 * (y * SIDE + x) + 3] = 0;
    const FamPicture terrain[TEXTURES] = { stage->terrain_pic[0], { SIDE, SIDE, gaps } };
    const FamSource src = { terrain, TEXTURES, NULL, 0, &stage->horizon_pic, NULL, NULL };
    uint8_t *file = NULL;
    size_t len = 0;
    char msg[256];
    CHECK(build(&src, &file, &len, msg, sizeof msg));
    if (file == NULL) return;
    Chunk root, group;
    CHECK(chunk_open(&root, file, len) == CHUNK_OK);
    CHECK(chunk_child(&root, FAM_CHILD_TERRAIN, &group) == CHUNK_OK);
    for (uint32_t b = 0; b < TEXTURES; b++) {
        Chunk bank;
        Fsh fsh;
        CHECK(chunk_child(&group, b, &bank) == CHUNK_OK && bank_open(&bank, &fsh));
        int bitmaps = 0, with_gaps = 0, with_seen = 0;
        for (uint32_t i = 0; i < fsh.count; i++) {
            FshEntry e;
            if (fsh_entry(&fsh, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP) continue;
            const size_t n = (size_t)e.width * e.height;
            int clear = 0;
            for (size_t k = 0; k < n; k++) clear += e.payload[k] == 0xff;
            ++bitmaps;
            with_gaps += clear > 0;
            with_seen += (size_t)clear < n;
        }
        CHECK(bitmaps == FAM_LETTERS * FAM_VARIANTS);
        CHECK_EQ_INT(with_gaps, b == 1 ? bitmaps : 0);
        CHECK_EQ_INT(with_seen, bitmaps);
    }
    free(file);
}

/* a fence's bank (terrain_fence): each letter it holds has the CD's two fence pictures, 000 at 64 x 64 and 001 at 80 x 40 (AL1's bank 16;
 * the game takes 001 for the variants 002..007 it does not find, track_art.c), where a terrain bank has the eight variants of the ladder */
static void test_fence_bank(const Stage *stage)
{
    const unsigned letters[TEXTURES] = { FAM_LETTERS_ALL, FAM_LETTER_A | FAM_LETTER_C };
    const unsigned char fence[TEXTURES] = { 0, 1 };
    const FamSource src = { stage->terrain_pic, TEXTURES, NULL, 0, &stage->horizon_pic, letters, fence };
    uint8_t *file = NULL;
    size_t len = 0;
    char msg[256];
    CHECK(build(&src, &file, &len, msg, sizeof msg));
    if (file == NULL) return;
    Chunk root, group, bank;
    Fsh fsh;
    CHECK(chunk_open(&root, file, len) == CHUNK_OK);
    CHECK(chunk_child(&root, FAM_CHILD_TERRAIN, &group) == CHUNK_OK);
    CHECK(chunk_child(&group, 0, &bank) == CHUNK_OK && bank_open(&bank, &fsh));
    CHECK_EQ_INT((int)fsh.count, 1 + FAM_LETTERS * FAM_VARIANTS);   /* the terrain bank as before */
    CHECK(chunk_child(&group, 1, &bank) == CHUNK_OK && bank_open(&bank, &fsh));
    CHECK_EQ_INT((int)fsh.count, 1 + 2 * 2);                         /* the palette, A000 A001 C000 C001 */
    static const char *const names[] = { "A000", "A001", "C000", "C001" };
    static const int sizes[][2] = { { 64, 64 }, { 80, 40 }, { 64, 64 }, { 80, 40 } };
    for (int i = 0; i < 4; i++) {
        FshEntry e;
        CHECK(find_entry(&fsh, names[i], &e));
        CHECK(e.width == sizes[i][0] && e.height == sizes[i][1]);
    }
    FshEntry none;
    CHECK(!find_entry(&fsh, "A002", &none) && !find_entry(&fsh, "B000", &none));
    free(file);
}

int main(int argc, char **argv)
{
    static Stage stage;
    stage_init(&stage);
    const FamSource src = { stage.terrain_pic, TEXTURES, stage.scenery_pic, 1, &stage.horizon_pic, NULL, NULL };
    uint8_t *file = NULL;
    size_t len = 0;
    char msg[256];
    if (!build(&src, &file, &len, msg, sizeof msg)) {
        fprintf(stderr, "build: %s\n", msg);
        return 1;
    }
    test_tree_and_banks(file, len);
    test_no_transparent_pixels(file, len);
    test_validator_accepts(file, len);
    test_same_input_same_bytes(&src, file, len);
    test_refusals(&stage);
    test_file_limit(&stage);
    test_letters(&stage, len);
    test_generic_check(&stage);
    test_see_through_bank(&stage);
    test_fence_bank(&stage);
    if (argc > 1) test_ladder_matches_cd(file, len, argv[1]);
    free(file);
    return TEST_REPORT();
}
