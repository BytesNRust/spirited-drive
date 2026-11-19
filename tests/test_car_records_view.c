/* owner 2026-11-18: the RECORDS screen's table (car_records_view.h): one row for each track of the list with the car's record there, a few rows
 * to a page, the pages going round, the pager's halves. */
#include "game/frontend/car_records_view.h"
#include "game/platform/platform.h"
#include "game/sys/race_records_added.h"
#include "game/sys/race_records_tracks.h"
#include "game/sys/text.h"
#include "game/sys/video.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size) { (void)name, (void)data, (void)size; return true; }
void platform_free_file(uint8_t *data) { free(data); }
bool platform_user_file_exists(const char *name) { (void)name; return false; }

/* the table's drawing is the screens' (the walks'): the text calls it makes are not the test's */
void text_select(TextStyle style) { (void)style; }
int text_width_scaled(const char *text, int num, int den) { (void)text, (void)num, (void)den; return 0; }
int text_width(const char *text) { (void)text; return 0; }
void text_draw_shadowed(const char *text, int x, int y) { (void)text, (void)x, (void)y; }
int text_mask_rows(const char *text, int num, int den) { (void)text, (void)num, (void)den; return 0; }
int text_mask_scaled(const char *text, int num, int den, uint8_t *mask, int mw, int mh) { (void)text, (void)num, (void)den, (void)mask, (void)mw, (void)mh; return 0; }
uint8_t *video_target(int *w, int *h) { *w = 0, *h = 0; return NULL; }
void text_draw_smooth(const char *text, int x, int y, int num, int den) { (void)text, (void)x, (void)y, (void)num, (void)den; }
void text_ink_scaled(const char *text, int num, int den, int *top, int *bottom) { (void)text, (void)num, (void)den, *top = 0, *bottom = 0; }

static void test_rows_and_pages(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    TrackList l;
    track_list_build(&l, NULL, 1, TRACK_LIST_LUNAR_NEVER);   /* the eight stock tracks and Lost Vegas */
    CarRecordsView *v = malloc(sizeof *v);
    CHECK(v != NULL);
    car_records_view_make(v, "f40", &l);
    CHECK_EQ_INT(v->count, l.count);
    CHECK(v->count >= 1);
    CHECK_EQ_INT(car_records_view_pages(v), (v->count + CAR_RECORDS_ROWS - 1) / CAR_RECORDS_ROWS);
    for (int i = 0; i < v->count; i++) CHECK(v->row[i].name[0] != 0 && !v->row[i].record.found);
    free(v);
}

static void test_pages_go_round(void)
{
    CarRecordsView v;
    memset(&v, 0, sizeof v);
    CHECK_EQ_INT(car_records_view_pages(&v), 1);   /* no rows: still a page */
    v.count = 2 * CAR_RECORDS_ROWS + 1;
    CHECK_EQ_INT(car_records_view_pages(&v), 3);
    car_records_view_step(&v, -1);
    CHECK_EQ_INT(v.page, 2);
    car_records_view_step(&v, 1);
    CHECK_EQ_INT(v.page, 0);
    car_records_view_step(&v, 1);
    CHECK_EQ_INT(v.page, 1);
}

static void test_reddish_pages(void)
{
    CHECK(car_records_reddish(72, 16, 16));    /* the 512TR's page: a dark red */
    CHECK(car_records_reddish(72, 30, 24));    /* a brownish red still */
    CHECK(!car_records_reddish(20, 72, 30));   /* the 911's green */
    CHECK(!car_records_reddish(72, 58, 42));   /* the menus' sepia (hue 30, 42% saturated) */
    CHECK(!car_records_reddish(24, 36, 72));   /* an added car's blue */
    CHECK(!car_records_reddish(0, 0, 0));
    CHECK(!car_records_reddish(60, 60, 60));
}

int main(void)
{
    test_rows_and_pages();
    test_pages_go_round();
    test_reddish_pages();
    return TEST_REPORT();
}
