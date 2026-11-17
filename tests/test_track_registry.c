/* dep59 step 5 (T3): the registry of added tracks (game/sys/track_registry.h). An added track gets the lowest free number from TRACK_ADDED_FIRST and
 * keeps it while it stays registered; a second id or stem, a stem track_piece_stem_ok refuses (the CD's AL, CL, CY, TR), a circuit, no
 * fingerprint and the 65th track are refused; an update keeps the number and a refused one leaves the track as it was. */
#include "game/sys/track_registry.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static TrackPiece piece(const char *id, const char *stem, uint32_t print)
{
    TrackPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.name, sizeof p.name, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.spec.kind = TRACK_KIND_STAGE, p.spec.segments = 1, p.spec.times = TRACK_TIME_DAY;
    p.fingerprint = print;
    return p;
}

static int add(const char *id, const char *stem, uint32_t print, char *why)
{
    const TrackPiece p = piece(id, stem, print);
    why[0] = '\0';
    return track_registry_add(&p, why, 160);
}

static void test_add_and_look_up(void)
{
    char why[160];
    track_registry_clear();
    CHECK_EQ_INT(track_registry_count(), 0);
    CHECK_EQ_INT(track_registry_is_added(16), 0);
    CHECK(track_registry_piece(16) == NULL);
    CHECK_EQ_INT(add("turin", "TURINI", 7, why), 16);
    CHECK_EQ_INT(add("alps-run", "PEAKS", 8, why), 17);
    CHECK_EQ_INT(track_registry_find("turin"), 16);
    CHECK_EQ_INT(track_registry_find("alps-run"), 17);
    CHECK_EQ_INT(track_registry_find("nope"), -1);
    CHECK_EQ_INT(track_registry_find(NULL), -1);
    CHECK(track_registry_piece(16) != NULL && strcmp(track_registry_piece(16)->stem, "TURINI") == 0);
    CHECK_EQ_INT(track_registry_is_added(15), 0);   /* the port's entries are not added tracks */
    CHECK_EQ_INT(track_registry_is_added(18), 0);
    CHECK_EQ_INT(track_registry_count(), 2);
    int16_t out[4];
    CHECK_EQ_INT(track_registry_tracks(out, 4), 2);
    CHECK(out[0] == 16 && out[1] == 17);
    CHECK_EQ_INT(track_registry_tracks(out, 1), 1);
}

static void test_refusals(void)
{
    char why[160];
    track_registry_clear();
    CHECK_EQ_INT(add("turin", "TURINI", 7, why), 16);
    CHECK_EQ_INT(add("turin", "OTHER", 7, why), -1);   /* the id again */
    CHECK(strstr(why, "turin") != NULL);
    CHECK_EQ_INT(add("other", "turini", 7, why), -1);   /* a lower case stem the rules refuse */
    CHECK_EQ_INT(add("other", "TURINI", 7, why), -1);   /* the stem again */
    CHECK(strstr(why, "another added track's") != NULL);
    CHECK_EQ_INT(add("other", "CYBER", 7, why), -1);    /* begins like the CD's */
    CHECK(strstr(why, "CD's") != NULL);
    CHECK_EQ_INT(add("other", "OTHER", 0, why), -1);    /* no fingerprint */
    CHECK_EQ_INT(add("", "OTHER", 7, why), -1);          /* no id */
    TrackPiece circuit = piece("ring", "RING", 7);
    circuit.spec.kind = TRACK_KIND_CIRCUIT;
    CHECK_EQ_INT(track_registry_add(&circuit, why, sizeof why), -1);
    CHECK(strstr(why, "A-to-B") != NULL);
    CHECK_EQ_INT(track_registry_count(), 1);
}

static void test_numbers_are_kept_and_reused_for_new_tracks_only(void)
{
    char why[160];
    track_registry_clear();
    CHECK_EQ_INT(add("a", "AAA", 1, why), 16);
    CHECK_EQ_INT(add("b", "BBB", 2, why), 17);
    CHECK_EQ_INT(add("c", "CCC", 3, why), 18);
    CHECK_EQ_INT(track_registry_remove("b"), 1);
    CHECK_EQ_INT(track_registry_remove("b"), 0);
    CHECK_EQ_INT(track_registry_find("c"), 18);   /* the others keep theirs */
    CHECK_EQ_INT(add("d", "DDD", 4, why), 17);    /* the lowest free one goes to a new track */
    CHECK_EQ_INT(add("e", "BBB", 5, why), 19);    /* b's stem was freed with it; its number went to d */
}

static void test_update(void)
{
    char why[160];
    track_registry_clear();
    CHECK_EQ_INT(add("a", "AAA", 1, why), 16);
    CHECK_EQ_INT(add("b", "BBB", 2, why), 17);
    TrackPiece p = piece("b", "BBB", 22);
    snprintf(p.length, sizeof p.length, "14.2 km");
    CHECK_EQ_INT(track_registry_update(&p, why, sizeof why), 17);   /* same number, new fingerprint */
    CHECK(track_registry_piece(17)->fingerprint == 22 && strcmp(track_registry_piece(17)->length, "14.2 km") == 0);
    p = piece("b", "AAA", 23);   /* a's stem */
    CHECK_EQ_INT(track_registry_update(&p, why, sizeof why), -1);
    CHECK(track_registry_piece(17)->fingerprint == 22 && strcmp(track_registry_piece(17)->stem, "BBB") == 0);   /* as it was */
    p = piece("zz", "ZZZ", 24);
    CHECK_EQ_INT(track_registry_update(&p, why, sizeof why), -1);   /* not registered */
    CHECK_EQ_INT(track_registry_count(), 2);
    p = piece("b", "BBB", 22);
    p.spec.road = TRACK_ROAD_DIRT;
    CHECK_EQ_INT(track_registry_update(&p, why, sizeof why), 17);   /* an own stem is no clash */
    CHECK_EQ_INT(track_registry_piece(17)->spec.road, TRACK_ROAD_DIRT);
}

static void test_the_most(void)
{
    char why[160], id[16], stem[16];
    track_registry_clear();
    for (int i = 0; i < TRACK_ADDED_MAX; i++) {
        snprintf(id, sizeof id, "t%d", i);
        snprintf(stem, sizeof stem, "S%d", i);
        CHECK_EQ_INT(add(id, stem, 1, why), TRACK_ADDED_FIRST + i);
    }
    CHECK_EQ_INT(add("one-more", "MORE", 1, why), -1);
    CHECK(strstr(why, "64") != NULL);
    CHECK_EQ_INT(track_registry_count(), TRACK_ADDED_MAX);
}

int main(void)
{
    test_add_and_look_up();
    test_refusals();
    test_numbers_are_kept_and_reused_for_new_tracks_only();
    test_update();
    test_the_most();
    return TEST_REPORT();
}
