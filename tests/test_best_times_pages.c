/* Best Times' pages with Restomod's records (best_times_pages.c): every field a carousel (past its last value the first, before its
 * first the last), the keys' row through the strip and the footer (a stage has no laps), a new track's laps and page, and the short
 * words of a row's conditions. */
#include "game/frontend/best_times_pages.h"
#include "game/frontend/record_conditions.h"
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

/* the block's words: weather (snow and the wet grip over the sky) and time on the first line, the difficulty's word on the second (MIXED
 * when the axes differ), "-" and nothing for an imported row */
static void check_lines(RecordMarks m, const char *first, const char *second)
{
    char a[16], b[16];
    record_conditions_lines(m, a, b, sizeof a);
    CHECK(strcmp(a, first) == 0);
    CHECK(strcmp(b, second) == 0);
}

static void test_the_conditions_words(void)
{
    check_lines(race_records_conditions_of(WEATHER_RAIN, 0, 0, LIGHT_TIME_DUSK, 2, 2, 2), "RAIN DUSK", "HARD");
    check_lines(race_records_conditions_of(WEATHER_CLEAR, 0, 0, LIGHT_TIME_DAY, 0, 0, 0), "CLR DAY", "ORIGINAL");
    check_lines(race_records_conditions_of(WEATHER_CLEAR, 1, 0, LIGHT_TIME_NIGHT, 1, 1, 1), "SNOW NGT", "EASY");
    check_lines(race_records_conditions_of(WEATHER_FOG, 0, 1, LIGHT_TIME_DAWN, 0, 1, 2), "WET DAWN", "MIXED");
    check_lines(race_records_conditions_of(WEATHER_STORM, 0, 0, LIGHT_TIME_DUSK, 0, 0, 0), "STRM DUSK", "ORIGINAL");
    const RecordMarks old = { 0 };
    check_lines(old, "-", "");
}

/* the block drawn: right edge on `right`, the second line centred under the first and RECORD_CONDITIONS_GAP rows under its letters at
 * every size, the block's letters centred on the row's capitals; a fake font whose capitals fill rows 4..18 at full size */
static struct { char text[4][16]; int32_t x[4], y[4], num[4], den[4]; int n; } pen_drawn;
static void pen_text(void *ctx, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)ctx;
    if (pen_drawn.n >= 4) return;
    snprintf(pen_drawn.text[pen_drawn.n], 16, "%s", text);
    pen_drawn.x[pen_drawn.n] = x, pen_drawn.y[pen_drawn.n] = y, pen_drawn.num[pen_drawn.n] = num, pen_drawn.den[pen_drawn.n] = den;
    pen_drawn.n++;
}
static int32_t pen_width(void *ctx, const char *text, int32_t num, int32_t den) { (void)ctx; return (int32_t)strlen(text) * 16 * num / den; }
static void pen_ink(void *ctx, const char *text, int32_t num, int32_t den, int32_t *top, int32_t *bottom)
{
    (void)ctx;
    const int dash = strcmp(text, "-") == 0;   /* the dash sits mid-line */
    *top = (dash ? 10 : 4) * num / den, *bottom = (dash ? 12 : 18) * num / den;
}
enum { ROW = 100, ROW_MIDDLE = ROW + (4 + 18) / 2 };

/* the rows between line 1's letters and line 2's, and the middle of the block's letters */
static void block_rows(int32_t *gap, int32_t *middle)
{
    int32_t t1, b1, t2, b2;
    pen_ink(NULL, pen_drawn.text[0], pen_drawn.num[0], pen_drawn.den[0], &t1, &b1);
    pen_ink(NULL, pen_drawn.text[1], pen_drawn.num[1], pen_drawn.den[1], &t2, &b2);
    *gap = (pen_drawn.y[1] + t2) - (pen_drawn.y[0] + b1);
    *middle = ((pen_drawn.y[0] + t1) + (pen_drawn.y[1] + b2)) / 2;
}

static void test_the_conditions_block(void)
{
    const ConditionsPen pen = { NULL, pen_text, pen_width, pen_ink };
    int32_t gap, middle;
    memset(&pen_drawn, 0, sizeof pen_drawn);
    record_conditions_draw(race_records_conditions_of(WEATHER_STORM, 0, 0, LIGHT_TIME_DUSK, 0, 0, 0), 400, ROW, 1, 2, &pen);
    CHECK_EQ_INT(pen_drawn.n, 2);
    CHECK(strcmp(pen_drawn.text[0], "STRM DUSK") == 0 && strcmp(pen_drawn.text[1], "ORIGINAL") == 0);
    CHECK_EQ_INT(pen_drawn.x[0], 400 - 72);                  /* the wider line ends on `right` */
    CHECK_EQ_INT(pen_drawn.x[1], 400 - 72 + (72 - 64) / 2);  /* the second centred under it */
    block_rows(&gap, &middle);
    CHECK_EQ_INT(gap, RECORD_CONDITIONS_GAP);
    CHECK(middle >= ROW_MIDDLE - 1 && middle <= ROW_MIDDLE + 1);
    CHECK(pen_drawn.num[1] == 1 && pen_drawn.den[1] == 2);
    /* the speed's smaller block: the same rows between its lines */
    memset(&pen_drawn, 0, sizeof pen_drawn);
    record_conditions_draw(race_records_conditions_of(WEATHER_CLEAR, 0, 0, LIGHT_TIME_DAY, 1, 1, 1), 400, ROW, 2, 5, &pen);
    CHECK_EQ_INT(pen_drawn.x[0], 400 - 44);                  /* "CLR DAY" at 2/5 is the wider: "EASY" centred under it */
    CHECK_EQ_INT(pen_drawn.x[1], 400 - 44 + (44 - 25) / 2);
    block_rows(&gap, &middle);
    CHECK_EQ_INT(gap, RECORD_CONDITIONS_GAP);
    CHECK(middle >= ROW_MIDDLE - 1 && middle <= ROW_MIDDLE + 1);
    /* an imported row: the dash alone, on the row's middle */
    memset(&pen_drawn, 0, sizeof pen_drawn);
    const RecordMarks old = { 0 };
    record_conditions_draw(old, 400, ROW, 1, 2, &pen);
    CHECK_EQ_INT(pen_drawn.n, 1);
    CHECK(strcmp(pen_drawn.text[0], "-") == 0 && pen_drawn.x[0] == 400 - 8);
    CHECK(pen_drawn.y[0] + 5 >= ROW_MIDDLE - 1 && pen_drawn.y[0] + 6 <= ROW_MIDDLE + 1);
}

int main(void)
{
    test_the_conditions_block();
    test_the_layout_is_a_carousel();
    test_the_cheats_turn_over_from_every_item();
    test_the_laps_are_a_carousel_of_the_counts_there_are();
    test_the_page_is_a_carousel();
    test_the_keys_row();
    test_another_track();
    test_the_conditions_words();
    return TEST_REPORT();
}
