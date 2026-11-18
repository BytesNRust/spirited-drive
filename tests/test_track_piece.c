/* dep59 step 5 (T3): the rules of a track piece (formats/track_piece.h): the stems it may take, the card's keys in numbers, the files of
 * each stage, which files it may carry, the check of a piece's files and the fingerprint that decides how it races. */
#include "formats/exchange.h"
#include "formats/mod_ini.h"
#include "formats/track_piece.h"
#include "formats/validate.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static ModInfo card_of(const char *text)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(mod_ini_parse((const uint8_t *)text, strlen(text), &m, why, sizeof why), 1);
    return m;
}

static int has(const Report *r, const char *id, const char *text)
{
    for (size_t i = 0; i < r->count; i++)
        if (strcmp(r->item[i].limit->id, id) == 0 && (text == NULL || strstr(r->item[i].item, text) != NULL)) return 1;
    return 0;
}

static void test_stems(void)
{
    static const char *const OK[] = { "TURINI", "A", "X1", "ABCDEFG", "T", "BEACH7" };
    static const char *const BAD[] = { "", "turini", "ABCDEFGH", "A/B", "..", "A B", "AL", "CL1", "CY", "TR", "TR8", "TRIANA", "ALPES", "CLIFF", "CYBER" };
    for (size_t i = 0; i < sizeof OK / sizeof OK[0]; i++) CHECK_EQ_INT(track_piece_stem_ok(OK[i]), 1);
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) CHECK_EQ_INT(track_piece_stem_ok(BAD[i]), 0);
}

static void test_spec_defaults_and_values(void)
{
    TrackPieceSpec s;
    Report r;
    report_init(&r);
    ModInfo m = card_of("id=a\nname=A\nversion=1\ntype=track\nstem=TURINI\n");
    CHECK_EQ_INT(track_piece_spec(&m, &s, &r), 1);
    CHECK_EQ_INT(s.kind, TRACK_KIND_STAGE);
    CHECK_EQ_INT(s.segments, 1);
    CHECK_EQ_INT(s.road, TRACK_ROAD_ASPHALT);
    CHECK_EQ_INT(s.edge, TRACK_EDGE_GRASS);
    CHECK_EQ_INT(s.times, TRACK_TIME_DAY);
    m = card_of("id=a\nname=A\nversion=1\ntype=track\nstem=TURINI\nkind=stage\nsegments=3\nroad=dirt\nedge=water\ntimes=dawn, day,night\n");
    CHECK_EQ_INT(track_piece_spec(&m, &s, &r), 1);
    CHECK_EQ_INT(s.segments, 3);
    CHECK_EQ_INT(s.road, TRACK_ROAD_DIRT);
    CHECK_EQ_INT(s.edge, TRACK_EDGE_WATER);
    CHECK_EQ_INT(s.times, TRACK_TIME_DAWN | TRACK_TIME_DAY | TRACK_TIME_NIGHT);
    CHECK_EQ_INT((int)report_fatal_count(&r), 0);
    report_free(&r);
}

static void test_spec_refusals(void)
{
    static const struct { const char *keys, *id, *text; } BAD[] = {
        { "type=car\n", "PIECE.TYPE", "not track" },
        { "kind=circuit\n", "PIECE.KIND", "A-to-B tracks only" },
        { "kind=rally\n", "PIECE.KIND", "kind=rally" },
        { "segments=0\n", "PIECE.SEGMENTS", "segments=0" },
        { "segments=4\n", "PIECE.SEGMENTS", "segments=4" },
        { "segments=12\n", "PIECE.SEGMENTS", "segments=12" },
        { "road=gravel\n", "PIECE.ROAD", "road=gravel" },
        { "edge=lava\n", "PIECE.EDGE", "edge=lava" },
        { "times=day,day\n", "PIECE.TIMES", "times=day,day" },
        { "times=noon\n", "PIECE.TIMES", "times=noon" },
        { "times=day,\n", "PIECE.TIMES", "times=day," },
        { "showcase_tint=red\n", "PIECE.TINT", "showcase_tint=red" },
        { "showcase_tint=#12345\n", "PIECE.TINT", "showcase_tint=#12345" },
        { "showcase_tint=#12345G\n", "PIECE.TINT", "#RRGGBB" },
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        char text[200];
        snprintf(text, sizeof text, "id=a\nname=A\nversion=1\ntype=track\nstem=TURINI\n%s", BAD[i].keys);
        if (strncmp(BAD[i].keys, "type=car", 8) == 0) snprintf(text, sizeof text, "id=a\nname=A\nversion=1\nstem=TURINI\n%s", BAD[i].keys);
        ModInfo m = card_of(text);
        TrackPieceSpec s;
        Report r;
        report_init(&r);
        CHECK_EQ_INT(track_piece_spec(&m, &s, &r), 0);
        CHECK(has(&r, BAD[i].id, BAD[i].text));
        report_free(&r);
    }
}

static void test_paths_and_ownership(void)
{
    char p[TRACK_PIECE_FILES][TRACK_PIECE_PATH];
    track_piece_paths("TURINI", 2, p);
    CHECK(strcmp(p[0], "SIMDATA/MISC/TURINI2.TRI") == 0 && strcmp(p[1], "SIMDATA/MISC/TURINI2INFO") == 0);
    CHECK(strcmp(p[2], "SIMDATA/ETRACKFM/TURINI2_001.FAM") == 0 && strcmp(p[3], "SIMDATA/GTRACKFM/TURINI2_001.FAM") == 0);
    static const char *const OWNED[] = {
        "SIMDATA/MISC/TURINI1.TRI", "simdata/misc/turini3.tri", "SIMDATA/MISC/TURINI2INFO", "SIMDATA/ETRACKFM/TURINI1_001.FAM",
        "SIMDATA/GTRACKFM/TURINI3_001.FAM", "SIMDATA/NTRACKFM/TURINI1_T01.FAM", "SIMDATA/NTRACKFM/TURINI2_M01.FAM",
        "SIMDATA/NTRACKFM/TURINI3_R01.FAM", "track/TURINI1.txt", "track/turini2m.txt", "track_light/TURINI3.txt",
        "track_masks/TURINI1/lamp.png", "track_masks/TURINI2m/lamp.png" };
    static const char *const NOT[] = {
        "SIMDATA/MISC/TURINI4.TRI", "SIMDATA/MISC/TURINI.TRI", "SIMDATA/MISC/CY1.TRI", "SIMDATA/MISC/TURINI10.TRI", "SIMDATA/MISC/TURINI1.TXT",
        "SIMDATA/ETRACKFM/TURINI1_002.FAM", "SIMDATA/NTRACKFM/TURINI1_X01.FAM", "SIMDATA/CARFAMS/TURINI1.CFM", "track/TURINI4.txt",
        "track/CY1.txt", "track/TURINI1.png", "track/sub/TURINI1.txt", "track_masks/TURINI1.png", "track_masks/TURINI1/a/b.png",
        "track_masks/CY1/lamp.png", "car_masks/TURINI1/x.png", "sounds/TURINI1.wav", "SIMDATA/MISC/OTHER1.TRI" };
    for (size_t i = 0; i < sizeof OWNED / sizeof OWNED[0]; i++) CHECK_EQ_INT(track_piece_owns("TURINI", OWNED[i]), 1);
    for (size_t i = 0; i < sizeof NOT / sizeof NOT[0]; i++) CHECK_EQ_INT(track_piece_owns("TURINI", NOT[i]), 0);
    CHECK_EQ_INT(track_piece_owns("CY", "SIMDATA/MISC/CY1.TRI"), 0);   /* a stem the rules refuse owns nothing */
}

/* a piece of `segments` stages whose files are the bytes `fill`: files that are present, not good ones (the check's own limits are validate_file's) */
static void add_stages(FileSet *set, const char *stem, int segments, int skip_path)
{
    static const uint8_t BYTES[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int index = 0;
    for (int stage = 1; stage <= segments; stage++) {
        char p[TRACK_PIECE_FILES][TRACK_PIECE_PATH];
        track_piece_paths(stem, stage, p);
        for (int i = 0; i < TRACK_PIECE_FILES; i++, index++)
            if (index != skip_path) CHECK_EQ_INT(fileset_add(set, p[i], BYTES, sizeof BYTES), 1);
    }
}

static void test_check_looks_for_every_file(void)
{
    ModInfo m = card_of("id=a\nname=A\nversion=1\ntype=track\nstem=TURINI\nsegments=2\n");
    FileSet set;
    Report r;
    fileset_init(&set);
    report_init(&r);
    CHECK_EQ_INT(track_piece_check(&m, &set, NULL, &r), 0);   /* nothing there: all eight are missing */
    int missing = 0;
    for (size_t i = 0; i < r.count; i++) missing += strcmp(r.item[i].limit->id, "PIECE.FILE_MISSING") == 0;
    CHECK_EQ_INT(missing, 8);
    report_free(&r);
    fileset_free(&set);

    fileset_init(&set);
    report_init(&r);
    add_stages(&set, "TURINI", 2, 4);   /* the second stage's TRI (the fifth file) left out */
    CHECK_EQ_INT(track_piece_check(&m, &set, NULL, &r), 0);
    CHECK(has(&r, "PIECE.FILE_MISSING", "SIMDATA/MISC/TURINI2.TRI"));
    CHECK(!has(&r, "PIECE.FILE_MISSING", "TURINI1"));
    report_free(&r);
    fileset_free(&set);
}

static void test_check_stops_at_a_card_that_is_not_good(void)
{
    ModInfo m = card_of("id=a\nname=A\nversion=1\ntype=track\nstem=CYBER\nkind=circuit\n");
    FileSet set;
    Report r;
    fileset_init(&set);
    report_init(&r);
    CHECK_EQ_INT(track_piece_check(&m, &set, NULL, &r), 0);
    CHECK(has(&r, "PIECE.KIND", NULL) && has(&r, "PIECE.STEM", "CYBER"));
    CHECK(!has(&r, "PIECE.FILE_MISSING", NULL));   /* no files looked for */
    report_free(&r);
    fileset_free(&set);
}

static void test_fingerprint(void)
{
    ModInfo m = card_of("id=a\nname=A\nversion=1\ntype=track\nstem=TURINI\nsegments=2\n");
    TrackPieceSpec s;
    Report r;
    report_init(&r);
    CHECK_EQ_INT(track_piece_spec(&m, &s, &r), 1);
    report_free(&r);
    FileSet set;
    fileset_init(&set);
    CHECK_EQ_INT((int)track_piece_fingerprint(&s, "TURINI", &set), 0);   /* a TRI missing */
    add_stages(&set, "TURINI", 2, -1);
    const uint32_t a = track_piece_fingerprint(&s, "TURINI", &set);
    CHECK(a != 0);
    CHECK(track_piece_fingerprint(&s, "TURINI", &set) == a);   /* stable */
    TrackPieceSpec dirt = s;
    dirt.road = TRACK_ROAD_DIRT;
    CHECK(track_piece_fingerprint(&dirt, "TURINI", &set) != a);   /* the card's options count */
    TrackPieceSpec edge = s;
    edge.edge = TRACK_EDGE_GRAVEL;
    CHECK(track_piece_fingerprint(&edge, "TURINI", &set) != a);
    TrackPieceSpec times = s;
    times.times = TRACK_TIME_NIGHT;
    CHECK(track_piece_fingerprint(&times, "TURINI", &set) == a);   /* the hours do not change how it races */
    fileset_free(&set);

    FileSet other;   /* one byte of the first TRI changed */
    fileset_init(&other);
    char p[TRACK_PIECE_FILES][TRACK_PIECE_PATH];
    static const uint8_t SAME[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, CHANGED[8] = { 1, 2, 3, 4, 5, 6, 7, 9 };
    for (int stage = 1; stage <= 2; stage++) {
        track_piece_paths("TURINI", stage, p);
        CHECK_EQ_INT(fileset_add(&other, p[0], stage == 1 ? CHANGED : SAME, 8), 1);
    }
    CHECK(track_piece_fingerprint(&s, "TURINI", &other) != a);
    fileset_free(&other);
}

/* port (newcomer pains): a showcase_tint that is not #RRGGBB is a fatal PIECE.TINT, so the piece is refused before any file is looked at */
static void test_a_bad_tint_is_refused(void)
{
    static const char *const TEXT = "id=bad-tint\nname=Bad\nversion=0.1\ntype=track\nstem=TINTY\nkind=stage\nsegments=1\n"
                                     "road=asphalt\nedge=grass\nshowcase_tint=orange\n";
    ModInfo card;
    char why[160];
    CHECK_EQ_INT(mod_ini_parse((const uint8_t *)TEXT, strlen(TEXT), &card, why, sizeof why), 1);
    FileSet set;
    fileset_init(&set);
    Report r;
    report_init(&r);
    CHECK_EQ_INT(track_piece_check(&card, &set, NULL, &r), 0);
    int found = 0;
    for (size_t i = 0; i < r.count; i++)
        if (strcmp(r.item[i].limit->id, "PIECE.TINT") == 0) found = r.item[i].limit->fatal;
    CHECK_EQ_INT(found, 1);
    report_free(&r);
    fileset_free(&set);
}

int main(void)
{
    test_a_bad_tint_is_refused();
    test_stems();
    test_spec_defaults_and_values();
    test_spec_refusals();
    test_paths_and_ownership();
    test_check_looks_for_every_file();
    test_check_stops_at_a_card_that_is_not_good();
    test_fingerprint();
    return TEST_REPORT();
}
