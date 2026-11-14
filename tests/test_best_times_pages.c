/* Best Times' pages with Restomod's records (best_times_pages.c): every field a carousel (past its last value the first, before its
 * first the last), the keys' row through the strip and the footer (a stage has no laps), a new track's laps and page, and the short
 * words of a row's conditions. */
#include "game/frontend/best_times_pages.h"
#include "game/race/weather.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return true;
}
void platform_free_file(uint8_t *data) { (void)data; }
bool platform_user_file_exists(const char *name)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data != NULL) platform_free_file(data);
    return data != NULL;
}

enum { STAGE = 1, CIRCUIT = 3 };   /* Coastal (open road); Rusty Springs: its counts 4, 8, 16 */

/* the layout goes round all eight both ways */
static void test_the_layout_is_a_carousel(void)
{
    BestTimesPlus p;
    best_times_pages_open(&p, CIRCUIT, 0, 0, 0, 1);
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_LAYOUT_DOWN);
    CHECK_EQ_INT(p.layout, RACE_RECORDS_LAYOUTS - 1);
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_LAYOUT_UP);
    CHECK_EQ_INT(p.layout, 0);
    for (int i = 0; i < RACE_RECORDS_LAYOUTS; i++) best_times_pages_act(&p, CIRCUIT, BEST_PAGES_LAYOUT_UP);
    CHECK_EQ_INT(p.layout, 0);
}

/* the cheats: its word and both its arrows turn it over */
static void test_the_cheats_turn_over_from_every_item(void)
{
    BestTimesPlus p;
    best_times_pages_open(&p, CIRCUIT, 0, 0, 0, 1);
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_CHEATS_DOWN);
    CHECK_EQ_INT(p.cheats, 1);
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_CHEATS_DOWN);
    CHECK_EQ_INT(p.cheats, 0);
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_CHEATS_UP);
    CHECK_EQ_INT(p.cheats, 1);
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_CHEATS);
    CHECK_EQ_INT(p.cheats, 0);
}

/* the laps step through the track's own counts and those with a table, and wrap round both ends */
static void test_the_laps_are_a_carousel_of_the_counts_there_are(void)
{
    race_records_reset();
    (void)race_records_make(CIRCUIT, 0, 0, 7);
    (void)race_records_make(CIRCUIT, 0, 0, 30);
    BestTimesPlus p;
    best_times_pages_open(&p, CIRCUIT, 0, 0, 0, 0);
    CHECK_EQ_INT(p.laps, 4);   /* the menu's choice 0 */
    static const int UP[] = { 7, 8, 16, 30, 4 };
    for (int i = 0; i < 5; i++) {
        best_times_pages_act(&p, CIRCUIT, BEST_PAGES_LAPS_UP);
        CHECK_EQ_INT(p.laps, UP[i]);
    }
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_LAPS_DOWN);
    CHECK_EQ_INT(p.laps, 30);
    best_times_pages_open(&p, CIRCUIT, 0, 1, 0, 0);   /* the cheats tables have none of those: the track's own */
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_LAPS_DOWN);
    CHECK_EQ_INT(p.laps, 16);
    best_times_pages_open(&p, CIRCUIT, 0, 0, 12, 0);   /* laps=12 shown though no table has it */
    CHECK_EQ_INT(p.laps, 12);
    best_times_pages_act(&p, CIRCUIT, BEST_PAGES_LAPS_UP);
    CHECK_EQ_INT(p.laps, 16);
    race_records_reset();
}

/* the page wraps both ways, two a track */
static void test_the_page_is_a_carousel(void)
{
    BestTimesPlus p;
    CHECK_EQ_INT(best_times_pages_count(STAGE), 2);
    CHECK_EQ_INT(best_times_pages_count(CIRCUIT), 2);
    best_times_pages_open(&p, STAGE, 0, 0, 0, 0);
    best_times_pages_act(&p, STAGE, BEST_PAGES_PAGE_DOWN);
    CHECK_EQ_INT(p.page, 1);
    best_times_pages_act(&p, STAGE, BEST_PAGES_PAGE_UP);
    CHECK_EQ_INT(p.page, 0);
    best_times_pages_act(&p, STAGE, BEST_PAGES_PAGE_UP);
    best_times_pages_act(&p, STAGE, BEST_PAGES_PAGE_UP);
    CHECK_EQ_INT(p.page, 0);
}

/* the keys' row: down from the footer round to the layout, up from the layout round to the footer; a stage's skips the laps; Left and
 * Right step the row's field, nothing on the footer */
static void test_the_keys_row(void)
{
    BestTimesPlus p;
    best_times_pages_open(&p, CIRCUIT, 0, 0, 0, 1);
    CHECK_EQ_INT(p.focus, BEST_FOCUS_FOOTER);
    best_times_pages_focus(&p, CIRCUIT, 1);
    CHECK_EQ_INT(p.focus, BEST_FOCUS_LAYOUT);
    best_times_pages_focus(&p, CIRCUIT, -1);
    CHECK_EQ_INT(p.focus, BEST_FOCUS_FOOTER);
    best_times_pages_focus(&p, CIRCUIT, -1);
    CHECK_EQ_INT(p.focus, BEST_FOCUS_PAGE);
    best_times_pages_focus(&p, CIRCUIT, -1);
    CHECK_EQ_INT(p.focus, BEST_FOCUS_LAPS);
    best_times_pages_step(&p, CIRCUIT, 1);
    CHECK_EQ_INT(p.laps, 16);
    best_times_pages_focus(&p, CIRCUIT, -1);
    best_times_pages_step(&p, CIRCUIT, -1);
    CHECK_EQ_INT(p.cheats, 1);
    best_times_pages_focus(&p, CIRCUIT, -1);
    best_times_pages_step(&p, CIRCUIT, -1);
    CHECK_EQ_INT(p.layout, RACE_RECORDS_LAYOUTS - 1);
    best_times_pages_open(&p, STAGE, 0, 0, 0, 1);
    best_times_pages_focus(&p, STAGE, -1);
    best_times_pages_focus(&p, STAGE, -1);
    CHECK_EQ_INT(p.focus, BEST_FOCUS_CHEATS);   /* over the laps */
    CHECK_EQ_INT(p.laps, 0);
    p.focus = BEST_FOCUS_FOOTER;
    const BestTimesPlus before = p;
    best_times_pages_step(&p, STAGE, 1);
    CHECK(memcmp(&p, &before, sizeof p) == 0);
}

/* another track: a stage has no laps, a circuit takes the menu's (or laps=N's); the keys leave a laps row a stage lacks */
static void test_another_track(void)
{
    BestTimesPlus p;
    best_times_pages_open(&p, CIRCUIT, 0, 0, 0, 2);
    CHECK_EQ_INT(p.laps, 16);
    p.focus = BEST_FOCUS_LAPS;
    best_times_pages_track(&p, STAGE, 0, 2);
    CHECK_EQ_INT(p.laps, 0);
    CHECK_EQ_INT(p.focus, BEST_FOCUS_PAGE);
    best_times_pages_track(&p, CIRCUIT, 9, 2);
    CHECK_EQ_INT(p.laps, 9);
}

/* the short words: weather (snow and the wet grip over the sky), time, difficulty (M when the axes differ), "-" for an imported row */
static void test_the_conditions_words(void)
{
    char text[24];
    best_times_conditions_text(race_records_conditions_of(WEATHER_RAIN, 0, 0, LIGHT_TIME_DUSK, 2, 2, 2), text, sizeof text);
    CHECK(strcmp(text, "RAIN DUSK H") == 0);
    best_times_conditions_text(race_records_conditions_of(WEATHER_CLEAR, 0, 0, LIGHT_TIME_DAY, 0, 0, 0), text, sizeof text);
    CHECK(strcmp(text, "CLR DAY O") == 0);
    best_times_conditions_text(race_records_conditions_of(WEATHER_CLEAR, 1, 0, LIGHT_TIME_NIGHT, 1, 1, 1), text, sizeof text);
    CHECK(strcmp(text, "SNOW NGT E") == 0);
    best_times_conditions_text(race_records_conditions_of(WEATHER_FOG, 0, 1, LIGHT_TIME_DAWN, 0, 1, 2), text, sizeof text);
    CHECK(strcmp(text, "WET DAWN M") == 0);
    best_times_conditions_text(race_records_conditions_of(WEATHER_STORM, 0, 0, LIGHT_TIME_DAY, 1, 1, 1), text, sizeof text);
    CHECK(strcmp(text, "STRM DAY E") == 0);
    const RecordMarks old = { 0 };
    best_times_conditions_text(old, text, sizeof text);
    CHECK(strcmp(text, "-") == 0);
}

int main(void)
{
    test_the_layout_is_a_carousel();
    test_the_cheats_turn_over_from_every_item();
    test_the_laps_are_a_carousel_of_the_counts_there_are();
    test_the_page_is_a_carousel();
    test_the_keys_row();
    test_another_track();
    test_the_conditions_words();
    return TEST_REPORT();
}
