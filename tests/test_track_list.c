/* dep59 step 4 (Task 2): the tracks the menus' arrows step through (game/frontend/track_list.h): the standard edition's 0..7, and Lost
 * Vegas (8) once it is unlocked; an edition's tracks= in its own order, the stock ones alone (stock_content.h), an id that names no stock
 * track left out, and the hidden track (9, no geometry on the CD) never listed. */
#include "game/frontend/track_list.h"
#include "formats/edition_ini.h"
#include "test.h"

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

static void test_standard(void)
{
    TrackList l;
    track_list_build(&l, NULL, 1);
    CHECK(listed(&l, (const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7, 8 }, 9));
    CHECK_EQ_INT(track_list_step(&l, 7, 1), 8);
    CHECK_EQ_INT(track_list_step(&l, 8, 1), 0);
    CHECK_EQ_INT(track_list_step(&l, 0, -1), 8);
    track_list_build(&l, NULL, 0);
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
    track_list_build(&l, &e, 0);   /* an edition lists Lost Vegas by its tracks=, unlocked or not */
    CHECK(listed(&l, (const int16_t[]){ 0, 8 }, 2));
    CHECK_EQ_INT(track_list_step(&l, 0, 1), 8);
    CHECK_EQ_INT(track_list_step(&l, 0, -1), 8);
    CHECK_EQ_INT(track_list_step(&l, 8, 1), 0);
    CHECK_EQ_INT(track_list_step(&l, 9, 1), 0);   /* not listed: the first */
    CHECK_EQ_INT(track_list_step(&l, 3, -1), 0);
    e = edition_of("name=Order\ntracks=rusty,city,coastal\n");
    track_list_build(&l, &e, 1);   /* its own order, Lost Vegas not added */
    CHECK(listed(&l, (const int16_t[]){ 3, 0, 1 }, 3));
    CHECK_EQ_INT(track_list_step(&l, 1, 1), 3);
    CHECK_EQ_INT(track_list_step(&l, 3, -1), 1);
    e = edition_of("name=Kept\ntracks=moon-base,alpine,hidden\n");
    track_list_build(&l, &e, 0);   /* an id that is no stock track is left out (the file keeps it); the hidden track, with no geometry, too */
    CHECK(listed(&l, (const int16_t[]){ 2 }, 1));
    e = edition_of("name=Moon only\ntracks=hidden\n");
    track_list_build(&l, &e, 0);   /* the hidden track alone: no track to race, an empty list */
    CHECK_EQ_INT(l.count, 0);
    CHECK_EQ_INT(track_list_step(&l, 9, 1), 0);
    e = edition_of("name=Cars only\ncars=rx7\n");
    track_list_build(&l, &e, 1);   /* no track at all: an empty list, and a step goes nowhere */
    CHECK_EQ_INT(l.count, 0);
    CHECK_EQ_INT(track_list_step(&l, 4, 1), 0);
}

static void test_full(void)
{
    TrackList l;
    Edition e = edition_of("name=All\ntracks=hidden,vegas,transtropolis,vertigo,burnt,autumn,rusty,alpine,coastal,city\n");
    track_list_build(&l, &e, 0);
    CHECK(listed(&l, (const int16_t[]){ 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 9));
    CHECK(l.count <= TRACK_LIST_MAX);
}

int main(void)
{
    test_standard();
    test_edition();
    test_full();
    return TEST_REPORT();
}
