/* dep59 step 5 (T3): the added tracks come from the mod folders in effect (game/sys/track_pieces.h). A folder whose mod.ini says type=track and
 * whose files pass track_piece_check is a track, in Restomod only; one that does not is refused with one log line, and a folder of another type,
 * or with no mod.ini, is passed over. A reload keeps the numbers of the tracks that stay; track_pieces_follow loads again only when the mods in
 * effect changed (mod_set_generation). The pieces are copies of the CD's City 1 and 2 (skipped without the CD, as test_car_pieces). */
#include "game/sys/mod_set.h"
#include "game/sys/port_profile.h"
#include "game/sys/track_pieces.h"
#include "game/sys/track_registry.h"
#include "formats/showcase_tint.h"
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

/* a piece folder <base>/<folder> of id `id`, stem `stem` and `stages` stages (the CD's CY1, CY2 renamed): its card (`extra` lines after it),
 * the file number `leave` of the 4 * stages left out (-1: none); its path in `out` */
static int piece(const char *folder, const char *id, const char *stem, int stages, const char *extra, int leave, char out[400])
{
    char card[600], src[600], dir[400], name[64];
    snprintf(out, 400, "%s/%s", base, folder);
    snprintf(card, sizeof card, "id=%s\nname=%s\nversion=1\ntype=track\nstem=%s\nsegments=%d\n%s", id, id, stem, stages, extra);
    write_text(out, "mod.ini", card);
    static const char *const SRC_DIR[4] = { "MISC", "MISC", "ETRACKFM", "GTRACKFM" };
    static const char *const SRC_END[4] = { ".TRI", "INFO", "_001.FAM", "_001.FAM" };
    for (int stage = 1; stage <= stages; stage++)
        for (int i = 0; i < 4; i++) {
            if ((stage - 1) * 4 + i == leave) continue;
            snprintf(src, sizeof src, "%s/original/SIMDATA/%s/CY%d%s", TNFS_SOURCE_DIR, SRC_DIR[i], stage, SRC_END[i]);
            snprintf(dir, sizeof dir, "%s/SIMDATA/%s", out, SRC_DIR[i]);
            snprintf(name, sizeof name, "%s%d%s", stem, stage, SRC_END[i]);
            if (!copy(src, dir, name)) return 0;
        }
    return 1;
}

static int load1(const char *dir, int restomod)
{
    const char *dirs[1] = { dir };
    return track_pieces_load(dirs, 1, restomod);
}

static void test_one_piece(void)
{
    char a[400];
    CHECK(piece("a", "test-track", "TESTTRK", 2, "length=14.2 km\nauthor=Someone\nroad=dirt\nedge=gravel\n", -1, a));
    track_registry_clear();
    CHECK_EQ_INT(load1(a, 1), 1);
    CHECK_EQ_INT(track_registry_find("test-track"), 16);
    const TrackPiece *t = track_registry_piece(16);
    CHECK(t != NULL && t->description[0] == '\0');   /* none given */
    CHECK(t != NULL && strcmp(t->stem, "TESTTRK") == 0 && strcmp(t->length, "14.2 km") == 0 && strcmp(t->author, "Someone") == 0);
    CHECK(t != NULL && t->spec.kind == TRACK_KIND_STAGE && t->spec.segments == 2 && t->spec.road == TRACK_ROAD_DIRT && t->spec.edge == TRACK_EDGE_GRAVEL);
    CHECK(t != NULL && t->fingerprint != 0 && strcmp(t->dir, a) == 0);
    CHECK_EQ_INT(load1(a, 1), 1);   /* again: the same */
    CHECK_EQ_INT(track_registry_find("test-track"), 16);
    CHECK(t != NULL && memcmp(t->tint, SHOWCASE_TINT_SEPIA, 3) == 0 && !t->tinted);   /* no showcase_tint: the sepia, as it is */
    CHECK_EQ_INT(load1(a, 0), 0);   /* not Restomod: none */
    CHECK_EQ_INT(track_registry_count(), 0);
    char b[400];
    CHECK(piece("b", "tint-track", "TINTTRK", 1, "showcase_tint=#336699\ndescription=A dry run.\n", -1, b));
    CHECK_EQ_INT(load1(b, 1), 1);
    const TrackPiece *u = track_registry_piece(track_registry_find("tint-track"));
    CHECK(u != NULL && strcmp(u->description, "A dry run.") == 0);
    CHECK(u != NULL && u->tinted && u->tint[0] == 0x33 && u->tint[1] == 0x66 && u->tint[2] == 0x99);
    CHECK(piece("c", "bad-tint", "BADTINT", 1, "showcase_tint=blue\n", -1, b));
    CHECK_EQ_INT(load1(b, 1), 0);   /* a tint that is not #RRGGBB: the piece is passed over */
    track_registry_clear();
}

static void test_refused_and_passed_over(void)
{
    char d[400];
    for (int leave = 0; leave < 8; leave++) {   /* each of the 8 files of two stages, missing: refused */
        CHECK(piece("lacks", "bad", "BADTRK", 2, "", leave, d));
        track_registry_clear();
        char gone[600];
        snprintf(gone, sizeof gone, "rm -rf '%s'", d);
        CHECK_EQ_INT(load1(d, 1), 0);
        CHECK_EQ_INT(track_registry_count(), 0);
        if (system(gone) != 0) fprintf(stderr, "could not remove %s\n", d);
    }
    static const struct { const char *folder, *stem, *extra; } BAD[] = {
        { "circuit", "BADTRK", "kind=circuit\n" }, { "stolen", "CYBER", "" }, { "edge", "BADTRK", "edge=lava\n" },
        { "times", "BADTRK", "times=noon\n" }, { "road", "BADTRK", "road=ice\n" },
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        CHECK(piece(BAD[i].folder, "bad", BAD[i].stem, 1, BAD[i].extra, -1, d));
        track_registry_clear();
        CHECK_EQ_INT(load1(d, 1), 0);
        CHECK_EQ_INT(track_registry_count(), 0);
    }
    snprintf(d, sizeof d, "%s/no_stem", base);   /* no stem */
    write_text(d, "mod.ini", "id=x\nname=X\nversion=1\ntype=track\n");
    CHECK_EQ_INT(load1(d, 1), 0);
    snprintf(d, sizeof d, "%s/tweak", base);      /* another type: not a track, no refusal */
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
    CHECK(piece("ra", "trk-a", "KRKA", 1, "", -1, a));
    CHECK(piece("rb", "trk-b", "KRKB", 1, "", -1, b));
    CHECK(piece("rc", "trk-c", "KRKC", 1, "", -1, c));
    CHECK(piece("rtwin", "trk-a", "KRKTWIN", 1, "", -1, twin));
    track_registry_clear();
    const char *both[2] = { a, b };
    CHECK_EQ_INT(track_pieces_load(both, 2, 1), 2);
    CHECK_EQ_INT(track_registry_find("trk-a"), 16);
    CHECK_EQ_INT(track_registry_find("trk-b"), 17);
    CHECK_EQ_INT(track_pieces_load(both, 2, 1), 2);   /* the same folders: the same numbers */
    CHECK_EQ_INT(track_registry_find("trk-b"), 17);
    const char *second[1] = { b };
    CHECK_EQ_INT(track_pieces_load(second, 1, 1), 1);   /* the first gone: the second keeps its number */
    CHECK_EQ_INT(track_registry_find("trk-a"), -1);
    CHECK_EQ_INT(track_registry_find("trk-b"), 17);
    const char *more[2] = { b, c };
    CHECK_EQ_INT(track_pieces_load(more, 2, 1), 2);   /* a new one takes the free number */
    CHECK_EQ_INT(track_registry_find("trk-c"), 16);
    const char *twins[2] = { a, twin };               /* two folders of one id: the first wins */
    CHECK_EQ_INT(track_pieces_load(twins, 2, 1), 1);
    CHECK(strcmp(track_registry_piece(track_registry_find("trk-a"))->stem, "KRKA") == 0);
}

static void test_the_fingerprint(void)
{
    char a[400], same[400], dirt[400];
    CHECK(piece("fp_a", "print", "PRINT", 1, "", -1, a));
    CHECK(piece("fp_same", "print", "PRINT", 1, "times=dawn,day\n", -1, same));   /* the hours do not change how it races */
    CHECK(piece("fp_dirt", "print", "PRINT", 1, "road=dirt\n", -1, dirt));
    track_registry_clear();
    CHECK_EQ_INT(load1(a, 1), 1);
    const uint32_t first = track_registry_piece(16)->fingerprint;
    CHECK_EQ_INT(load1(same, 1), 1);
    CHECK(track_registry_piece(16)->fingerprint == first);
    CHECK_EQ_INT(load1(dirt, 1), 1);
    CHECK(track_registry_piece(16)->fingerprint != first);
    CHECK_EQ_INT(track_registry_find("print"), 16);   /* all along the same track */
}

static void test_it_follows_the_mods(void)
{
    char a[400];
    CHECK(piece("fa", "follow-track", "FOLLOW", 1, "", -1, a));
    track_registry_clear();
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(mod_set_start(cd, a));   /* --mods: the piece's folder */
    track_pieces_follow();
    CHECK_EQ_INT(track_registry_find("follow-track"), 16);
    track_registry_clear();
    track_pieces_follow();   /* the mods did not change: nothing is loaded again */
    CHECK_EQ_INT(track_registry_find("follow-track"), -1);
    CHECK(port_profile_select(PROFILE_STOCK));   /* Stock: no mods, no added track */
    track_registry_clear();
    TrackPiece stale;
    memset(&stale, 0, sizeof stale);
    snprintf(stale.id, sizeof stale.id, "stale");
    snprintf(stale.stem, sizeof stale.stem, "STALE");
    stale.spec.kind = TRACK_KIND_STAGE, stale.spec.segments = 1, stale.spec.times = TRACK_TIME_DAY, stale.fingerprint = 1;
    CHECK(track_registry_add(&stale, NULL, 0) == 16);
    track_pieces_follow();
    CHECK_EQ_INT(track_registry_count(), 0);
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    track_pieces_follow();
    CHECK_EQ_INT(track_registry_find("follow-track"), 16);
    mod_set_stop();
}

int main(void)
{
    char probe[600];
    snprintf(probe, sizeof probe, "%s/original/SIMDATA/MISC/CY1.TRI", TNFS_SOURCE_DIR);
    if (access(probe, R_OK) != 0) {
        fprintf(stderr, "track_pieces: no CD in original/, skipped\n");
        return TEST_REPORT();
    }
    snprintf(base, sizeof base, "%s/tnfs_track_pieces_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(cd, sizeof cd, "%s/cd", base);
    snprintf(pref, sizeof pref, "%s/pref", base);
    make_dirs(cd);
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_one_piece();
    test_refused_and_passed_over();
    test_numbers_stay_across_reloads();
    test_the_fingerprint();
    test_it_follows_the_mods();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
