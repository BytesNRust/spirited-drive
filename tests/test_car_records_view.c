/* owner 2026-11-18: the RECORDS screen's table (car_records_view.h): one row for each track of the list with the car's record there, a few rows
 * to a page, the pages going round, the pager's halves. */
#include "game/frontend/car_records_view.h"
#include "game/platform/platform.h"
#include "game/sys/race_records_added.h"
#include "game/sys/race_records_tracks.h"
#include "game/sys/text.h"
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
    CHECK(v->count > CAR_RECORDS_ROWS);
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

static void test_pager_halves(void)
{
    CarRecordsView v;
    memset(&v, 0, sizeof v);
    const int mid = (CAR_RECORDS_LEFT + CAR_RECORDS_RIGHT) / 2;
    CHECK_EQ_INT(car_records_view_hit(&v, mid - 20, CAR_RECORDS_PAGER_Y + 6), -1);
    CHECK_EQ_INT(car_records_view_hit(&v, mid + 20, CAR_RECORDS_PAGER_Y + 6), 1);
    CHECK_EQ_INT(car_records_view_hit(&v, mid - 20, CAR_RECORDS_ROW0), 0);
    CHECK_EQ_INT(car_records_view_hit(&v, 5, CAR_RECORDS_PAGER_Y + 6), 0);
}

int main(void)
{
    test_rows_and_pages();
    test_pages_go_round();
    test_pager_halves();
    return TEST_REPORT();
}
