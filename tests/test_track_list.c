/* dep59 step 4 (Task 2): the tracks the menus' arrows step through (game/frontend/track_list.h): the standard edition's 0..7, and Lost
 * Vegas (8) once it is unlocked; an edition's tracks= in its own order, the stock ones alone (stock_content.h), an id that names no stock
 * track left out, and the hidden track (9, no geometry on the CD) never listed. */
#include "game/frontend/track_list.h"
#include "formats/edition_ini.h"
#include "game/sys/track_registry.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int listed(const TrackList *l, const int16_t *want, int n)
{
    if (l->count != n) return 0;
    for (int i = 0; i < n; i++)
        if (l->track[i] != want[i]) return 0;
    return 1;
}

static Edition edition_of(const char *text)
{
    Edition e;
    char why[128];
    CHECK(edition_parse((const uint8_t *)text, strlen(text), &e, why, sizeof why));
    return e;
}

static int add_track(const char *id, const char *stem)
{
    TrackPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id), snprintf(p.name, sizeof p.name, "%s", id), snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.spec.kind = TRACK_KIND_STAGE, p.spec.segments = 1, p.spec.times = TRACK_TIME_DAY, p.fingerprint = 7;
    return track_registry_add(&p, NULL, 0);
}

/* dep59 step 5, T7b: a registered added track in tracks= is listed at its place by its number (16..79); an id that is registered by nobody is left out */
static void test_added(void)
{
    TrackList l;
    track_registry_clear();
    const int a = add_track("alp-pass", "ZZA"), b = add_track("dune", "ZZB");
    CHECK(a >= TRACK_ADDED_FIRST && b > a);
    Edition e = edition_of("name=Mixed\ntracks=city,dune,nothere,alp-pass,vegas\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);
    CHECK(listed(&l, (const int16_t[]){ 0, (int16_t)b, (int16_t)a, 8 }, 4));
    CHECK_EQ_INT(track_list_step(&l, (int16_t)b, 1), a);   /* the arrows pass through them */
    CHECK_EQ_INT(track_list_step(&l, 0, -1), 8);
    CHECK(track_list_has(&l, (int16_t)a));
    track_list_build(&l, NULL, 1, TRACK_LIST_LUNAR_ON);   /* the standard edition never lists them */
    CHECK(!track_list_has(&l, (int16_t)a) && !track_list_has(&l, (int16_t)b));
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);
    track_list_stock_only(&l);   /* the lists of the screens whose tables are the original's nine */
    CHECK(listed(&l, (const int16_t[]){ 0, 8 }, 2));
    track_registry_clear();
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);   /* the mod gone: the ids left out, the file keeps them */
    CHECK(listed(&l, (const int16_t[]){ 0, 8 }, 2));
}

static void test_standard(void)
{
    TrackList l;
    track_list_build(&l, NULL, 1, TRACK_LIST_LUNAR_OFF);
    CHECK(listed(&l, (const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7, 8 }, 9));
    CHECK_EQ_INT(track_list_step(&l, 7, 1), 8);
    CHECK_EQ_INT(track_list_step(&l, 8, 1), 0);
    CHECK_EQ_INT(track_list_step(&l, 0, -1), 8);
    track_list_build(&l, NULL, 0, TRACK_LIST_LUNAR_OFF);
    CHECK(listed(&l, (const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7 }, 8));
    CHECK_EQ_INT(track_list_step(&l, 7, 1), 0);
    CHECK_EQ_INT(track_list_step(&l, 0, -1), 7);
    CHECK_EQ_INT(track_list_step(&l, 3, 1), 4);
    CHECK_EQ_INT(track_list_step(&l, 3, -1), 2);
    CHECK_EQ_INT(track_list_step(&l, 8, 1), 0);   /* Lost Vegas still locked: not in the list, its first */
    CHECK(track_list_has(&l, 7) && !track_list_has(&l, 8) && !track_list_has(&l, 9));
    CHECK_EQ_INT(track_list_step(&l, 9, 1), 0);   /* the hidden track, never listed in the standard edition */
}

static void test_edition(void)
{
    TrackList l;
    Edition e = edition_of("name=Two\ntracks=city,vegas\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);   /* an edition lists Lost Vegas by its tracks=, unlocked or not */
    CHECK(listed(&l, (const int16_t[]){ 0, 8 }, 2));
    CHECK_EQ_INT(track_list_step(&l, 0, 1), 8);
    CHECK_EQ_INT(track_list_step(&l, 0, -1), 8);
    CHECK_EQ_INT(track_list_step(&l, 8, 1), 0);
    CHECK_EQ_INT(track_list_step(&l, 9, 1), 0);   /* not listed: the first */
    CHECK_EQ_INT(track_list_step(&l, 3, -1), 0);
    e = edition_of("name=Order\ntracks=rusty,city,coastal\n");
    track_list_build(&l, &e, 1, TRACK_LIST_LUNAR_OFF);   /* its own order, Lost Vegas not added */
    CHECK(listed(&l, (const int16_t[]){ 3, 0, 1 }, 3));
    CHECK_EQ_INT(track_list_step(&l, 1, 1), 3);
    CHECK_EQ_INT(track_list_step(&l, 3, -1), 1);
    e = edition_of("name=Kept\ntracks=moon-base,alpine,hidden\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);   /* an id that is no stock track is left out (the file keeps it); the hidden track, with no geometry, too */
    CHECK(listed(&l, (const int16_t[]){ 2 }, 1));
    e = edition_of("name=Moon only\ntracks=hidden\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);   /* the hidden track alone: no track to race, an empty list */
    CHECK_EQ_INT(l.count, 0);
    CHECK_EQ_INT(track_list_step(&l, 9, 1), 0);
    e = edition_of("name=Cars only\ncars=rx7\n");
    track_list_build(&l, &e, 1, TRACK_LIST_LUNAR_OFF);   /* no track at all: an empty list, and a step goes nowhere */
    CHECK_EQ_INT(l.count, 0);
    CHECK_EQ_INT(track_list_step(&l, 4, 1), 0);
}

/* dep59 Lunar: Lunar Springs (10) right after Rusty: in the standard edition with its switch on (level 2), in an own edition by its
 * tracks= whatever the switch; never in a tournament */
static void test_lunar(void)
{
    TrackList l;
    track_list_build(&l, NULL, 1, TRACK_LIST_LUNAR_ON);
    CHECK(listed(&l, (const int16_t[]){ 0, 1, 2, 3, 10, 4, 5, 6, 7, 8 }, 10));
    CHECK_EQ_INT(track_list_step(&l, 3, 1), 10);
    CHECK_EQ_INT(track_list_step(&l, 10, 1), 4);
    CHECK_EQ_INT(track_list_step(&l, 4, -1), 10);
    track_list_build(&l, NULL, 0, TRACK_LIST_LUNAR_OFF);   /* the switch off: no Lunar */
    CHECK(!track_list_has(&l, 10));
    CHECK_EQ_INT(track_list_step(&l, 10, 1), 0);
    track_list_build(&l, NULL, 0, TRACK_LIST_LUNAR_NEVER);   /* a tournament */
    CHECK(listed(&l, (const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7 }, 8));
    Edition e = edition_of("name=Moon\ntracks=city,lunar,rusty\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);   /* listed: shown with the switch off, in the file's order */
    CHECK(listed(&l, (const int16_t[]){ 0, 10, 3 }, 3));
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_ON);
    CHECK(listed(&l, (const int16_t[]){ 0, 10, 3 }, 3));
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_NEVER);
    CHECK(listed(&l, (const int16_t[]){ 0, 3 }, 2));
    e = edition_of("name=Rusty\ntracks=rusty,city\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_ON);   /* not listed: the switch does not add it to an edition */
    CHECK(listed(&l, (const int16_t[]){ 3, 0 }, 2));
    e = edition_of("name=Only\ntracks=lunar\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);   /* alone: it races on Rusty's geometry, on the CD */
    CHECK(listed(&l, (const int16_t[]){ 10 }, 1));
}

static void test_full(void)
{
    TrackList l;
    Edition e = edition_of("name=All\ntracks=hidden,vegas,transtropolis,vertigo,burnt,autumn,lunar,rusty,alpine,coastal,city\n");
    track_list_build(&l, &e, 0, TRACK_LIST_LUNAR_OFF);
    CHECK(listed(&l, (const int16_t[]){ 8, 7, 6, 5, 4, 10, 3, 2, 1, 0 }, 10));
    CHECK(l.count <= TRACK_LIST_MAX);
}

int main(void)
{
    test_added();
    test_standard();
    test_edition();
    test_lunar();
    test_full();
    return TEST_REPORT();
}
