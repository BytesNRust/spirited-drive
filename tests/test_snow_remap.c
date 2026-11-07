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

static void put(TrackArt *art, int idx, int r, int g, int b) { art->palette[idx * 3] = (uint8_t)r, art->palette[idx * 3 + 1] = (uint8_t)g, art->palette[idx * 3 + 2] = (uint8_t)b; }

/* the snow slot 36 of one 4x1 bitmap per variant (green, white, grey, transparent), the road slot 3's own of `own_w` x 1 (what the track's bank holds): the road takes the snow */
static void snow_road_art(TrackArt *art, const uint8_t *snow, uint8_t own[TRACK_TEXTURE_VARIANTS][4], int own_w, uint8_t map[TRACK_TEXTURE_SLOTS])
{
    put(art, 1, 40, 160, 40), put(art, 2, 240, 240, 240), put(art, 3, 100, 100, 100), put(art, 4, 40, 40, 40);
    art->has_palette = 1;
    for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++) {
        memset(own[v], 9, 4);
        art->slots[36][v].present = 1;
        art->slots[36][v].bitmap.width = 4, art->slots[36][v].bitmap.height = 1;
        art->slots[36][v].bitmap.payload = snow, art->slots[36][v].bitmap.payload_size = 4;
        art->slots[3][v].present = 1;
        art->slots[3][v].bitmap.width = (uint16_t)own_w, art->slots[3][v].bitmap.height = 1;
        art->slots[3][v].bitmap.payload = own[v], art->slots[3][v].bitmap.payload_size = (size_t)own_w;
    }
    memset(map, 0, TRACK_TEXTURE_SLOTS);
    map[3] = 36 + 1;
}

static void test_the_snow_road_loses_its_green(void)
{
    const uint8_t snow[4] = { 1, 2, 3, 0xFF };
    uint8_t own[TRACK_TEXTURE_VARIANTS][4], map[TRACK_TEXTURE_SLOTS];
    TrackArt *art = calloc(1, sizeof *art);
    snow_road_art(art, snow, own, 4, map);
    CHECK_EQ_INT(track_art_remap_slots_as(art, map, SNOW_ROAD_GREY), 1);
    for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++) {
        CHECK(art->slots[3][v].bitmap.payload == own[v]);       /* the road keeps its own place in the track's bank (the palette remap reaches it there) */
        CHECK_EQ_INT(own[v][0], 3);      /* the green is the grey of its own brightness */
        CHECK_EQ_INT(own[v][1], 2);      /* what was not green is as it was */
        CHECK_EQ_INT(own[v][2], 3);
        CHECK_EQ_INT(own[v][3], 0xFF);   /* and a transparent pixel stays one */
    }
    CHECK_EQ_INT(art->slots[3][0].bitmap.width, 4);   /* the snow's size is the slot's */
    CHECK_EQ_INT(snow[0], 1);            /* the snow slot the scenery uses is not touched */
    free(art);
}

static void test_the_pure_snow_road_is_white(void)
{
    const uint8_t snow[4] = { 1, 2, 3, 0xFF };
    uint8_t own[TRACK_TEXTURE_VARIANTS][4], map[TRACK_TEXTURE_SLOTS];
    TrackArt *art = calloc(1, sizeof *art);
    snow_road_art(art, snow, own, 4, map);
    CHECK_EQ_INT(track_art_remap_slots_as(art, map, SNOW_ROAD_PURE), 1);
    CHECK_EQ_INT(own[0][0], 2);          /* the green is snow-white */
    CHECK_EQ_INT(own[0][2], 3);          /* the rest is as it was */
    free(art);
}

static void test_a_road_with_no_room_or_no_mode_takes_the_snow_as_it_is(void)
{
    const uint8_t snow[4] = { 1, 2, 3, 0xFF };
    uint8_t own[TRACK_TEXTURE_VARIANTS][4], map[TRACK_TEXTURE_SLOTS];
    TrackArt *art = calloc(1, sizeof *art);
    snow_road_art(art, snow, own, 2, map);   /* its own bitmap has no room for the snow's pixels */
    CHECK_EQ_INT(track_art_remap_slots_as(art, map, SNOW_ROAD_GREY), 1);
    CHECK(art->slots[3][0].bitmap.payload == snow);
    CHECK_EQ_INT(own[0][0], 9);
    snow_road_art(art, snow, own, 4, map);
    CHECK_EQ_INT(track_art_remap_slots_as(art, map, SNOW_ROAD_AS_IS), 1);
    CHECK(art->slots[3][0].bitmap.payload == snow);
    CHECK_EQ_INT(own[0][0], 9);
    art->has_palette = 0;                    /* no palette: nothing to tell green by */
    snow_road_art(art, snow, own, 4, map);
    art->has_palette = 0;
    CHECK_EQ_INT(track_art_remap_slots_as(art, map, SNOW_ROAD_GREY), 1);
    CHECK(art->slots[3][0].bitmap.payload == snow);
    free(art);
}

int main(void)
{
    test_a_mapped_slot_takes_the_snows_bitmaps();
    test_a_chain_of_pairs_reads_every_slot_as_it_was();
    test_no_map_changes_nothing();
    test_the_snow_road_loses_its_green();
    test_the_pure_snow_road_is_white();
    test_a_road_with_no_room_or_no_mode_takes_the_snow_as_it_is();
    test_the_places_file_gives_the_map();
    return TEST_REPORT();
}
