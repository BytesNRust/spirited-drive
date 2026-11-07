/* port (snow_weather): the snow textures laid over the road's (track_art_remap_slots) and the places file's `snow_map` lines that say which */
#include "tests/test.h"

#include "game/race/track_art.h"
#include "game/race/track_light_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(PSX_LIGHT_SNOW_SLOTS == TRACK_TEXTURE_SLOTS, "the places file's snow map is a texture slot table");

static TrackArt *make_art(void)
{
    TrackArt *art = calloc(1, sizeof *art);
    for (int slot = 0; slot < TRACK_TEXTURE_SLOTS; slot++)
        for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++) {
            art->slots[slot][v].present = slot != 40;   /* slot 40: no bitmap */
            art->slots[slot][v].transparent = slot == 4 && v == 2;
            art->slots[slot][v].bitmap.width = 64, art->slots[slot][v].bitmap.height = 64 + slot;   /* the slot's mark */
        }
    return art;
}

static void test_a_mapped_slot_takes_the_snows_bitmaps(void)
{
    TrackArt *art = make_art();
    uint8_t map[TRACK_TEXTURE_SLOTS] = { 0 };
    map[3] = 4 + 1, map[6] = 40 + 1, map[143] = 143 + 1;   /* the road to the snow; to a slot with no bitmap; to itself */
    CHECK_EQ_INT(track_art_remap_slots(art, map), 1);      /* the one with no bitmap is left alone, and to itself changes nothing: neither is counted */
    for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++) {
        CHECK_EQ_INT(art->slots[3][v].bitmap.height, 64 + 4);        /* every variant is the snow's */
        CHECK_EQ_INT(art->slots[3][v].present, 1);
        CHECK_EQ_INT(art->slots[3][v].transparent, v == 2);          /* and its flags with it */
        CHECK_EQ_INT(art->slots[6][v].bitmap.height, 64 + 6);        /* untouched: its snow slot has no bitmap */
        CHECK_EQ_INT(art->slots[4][v].bitmap.height, 64 + 4);        /* the snow slot itself is unchanged */
        CHECK_EQ_INT(art->slots[5][v].bitmap.height, 64 + 5);        /* what is not in the map is unchanged */
    }
    free(art);
}

static void test_a_chain_of_pairs_reads_every_slot_as_it_was(void)
{
    TrackArt *art = make_art();
    uint8_t map[TRACK_TEXTURE_SLOTS] = { 0 };
    map[3] = 4 + 1, map[4] = 5 + 1;   /* 3 takes 4 and 4 takes 5: 3 gets the original 4, whichever is done first */
    CHECK_EQ_INT(track_art_remap_slots(art, map), 2);
    CHECK_EQ_INT(art->slots[3][0].bitmap.height, 64 + 4);
    CHECK_EQ_INT(art->slots[4][0].bitmap.height, 64 + 5);
    free(art);
}

static void test_no_map_changes_nothing(void)
{
    TrackArt *art = make_art(), *same = make_art();
    const uint8_t none[TRACK_TEXTURE_SLOTS] = { 0 };
    CHECK_EQ_INT(track_art_remap_slots(art, none), 0);
    CHECK_EQ_INT(track_art_remap_slots(art, NULL), 0);
    CHECK_EQ_INT(track_art_remap_slots(NULL, none), 0);
    CHECK(memcmp(art, same, sizeof *art) == 0);
    free(art), free(same);
}

static void test_the_places_file_gives_the_map(void)
{
    const char *text = "# tnfs track places 1\ntrack AL3\npresent tunnel\ntunnel 5 9\nsnow 1\nsnow_map 3 4\nsnow_map 9 10\n";
    PsxLight places = { 0 };
    int bad = -1;
    CHECK(track_light_place_parse(text, strlen(text), "AL3", 100, &places, &bad));
    CHECK_EQ_INT(places.snow, 1);
    places.nodes = 100;   /* the parse's caller's: what the writer needs to know the track's length */
    CHECK_EQ_INT(places.snow_map[3], 4 + 1);
    CHECK_EQ_INT(places.snow_map[9], 10 + 1);
    CHECK_EQ_INT(places.snow_map[4], 0);
    char *out = NULL;
    size_t size = 0;
    CHECK(track_light_place_format(&places, "AL3", &out, &size));   /* and writes them back */
    CHECK(strstr(out, "snow 1\n") != NULL && strstr(out, "snow_map 3 4\n") != NULL && strstr(out, "snow_map 9 10\n") != NULL);
    free(out);
    psx_light_free(&places);
    const char *bad_texts[] = { "snow_map 3\n", "snow_map 3 144\n", "snow_map 144 3\n", "snow_map a b\n", "snow_map 3 4 5\n", "snow 2\n" };
    const char *crlf = "# tnfs track places 1\r\ntrack AL3\r\nsnow_map 3 4\r\n";
    PsxLight c = { 0 };
    CHECK(track_light_place_parse(crlf, strlen(crlf), "AL3", 100, &c, NULL) && c.snow_map[3] == 5);   /* a CRLF file is as good */
    psx_light_free(&c);
    for (size_t i = 0; i < sizeof bad_texts / sizeof *bad_texts; i++) {
        char file[128];
        snprintf(file, sizeof file, "# tnfs track places 1\ntrack AL3\n%s", bad_texts[i]);
        PsxLight p = { 0 };
        int line = 0;
        CHECK(!track_light_place_parse(file, strlen(file), "AL3", 100, &p, &line) && line == 3);   /* a bad line, the third */
    }
}

int main(void)
{
    test_a_mapped_slot_takes_the_snows_bitmaps();
    test_a_chain_of_pairs_reads_every_slot_as_it_was();
    test_no_map_changes_nothing();
    test_the_places_file_gives_the_map();
    return TEST_REPORT();
}
