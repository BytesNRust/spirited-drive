/* dep59 step 5 (T7d): an added track's page, its state and keys (track_page.h): the rows that apply, the values the keys change, the
 * bottom row (DONE, Road/track's arrows, Best times) and what a click hits. */
#include "game/frontend/track_page.h"
#include "formats/track_piece.h"
#include "game/race/weather.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include <string.h>

enum { UP = TRACK_PAGE_KEY_UP, DOWN = TRACK_PAGE_KEY_DOWN, LEFT = TRACK_PAGE_KEY_LEFT, RIGHT = TRACK_PAGE_KEY_RIGHT, ENTER = TRACK_PAGE_KEY_ENTER,
       ESC = TRACK_PAGE_KEY_ESCAPE };

static TrackPage page(int times, int rally, int light, int weather_row)
{
    const TrackPageOffer offer = { times, rally, light, weather_row };
    TrackPage p;
    track_page_open(&p, &offer, &(TrackPageValues){ 0, 0, LIGHT_TIME_DAY, WEATHER_CLEAR });
    return p;
}

static void test_the_rows_that_apply(void)
{
    TrackPage p = page(TRACK_TIME_DAY, 0, 1, 1);   /* one time of day: no row; no _R01 art: no Rally */
    CHECK(track_page_shown(&p, TP_MIRROR) && !track_page_shown(&p, TP_RALLY) && !track_page_shown(&p, TP_TIME) && track_page_shown(&p, TP_WEATHER));
    CHECK(track_page_shown(&p, TP_DONE) && track_page_shown(&p, TP_PREVIOUS) && track_page_shown(&p, TP_NEXT) && track_page_shown(&p, TP_BEST));
    p = page(TRACK_TIME_DAY | TRACK_TIME_NIGHT, 1, 1, 1);
    CHECK(track_page_shown(&p, TP_RALLY) && track_page_shown(&p, TP_TIME));
    p = page(TRACK_TIME_DAY | TRACK_TIME_NIGHT, 1, 0, 1);   /* no light effects: no time of day */
    CHECK(!track_page_shown(&p, TP_TIME));
    p = page(TRACK_TIME_DAY, 1, 1, 0);   /* no weather row */
    CHECK(!track_page_shown(&p, TP_WEATHER));
    CHECK_EQ_INT(p.item, TP_MIRROR);   /* it opens on the first row */
}

static void test_the_values_the_keys_change(void)
{
    TrackPage p = page(TRACK_TIME_DAWN | TRACK_TIME_DAY | TRACK_TIME_NIGHT, 1, 1, 1);
    CHECK_EQ_INT(track_page_input(&p, RIGHT), TRACK_PAGE_STAY);   /* Mirror on */
    CHECK_EQ_INT(p.v.mirror, 1);
    track_page_input(&p, LEFT);
    CHECK_EQ_INT(p.v.mirror, 0);
    track_page_input(&p, ENTER);   /* Enter turns it the same way */
    CHECK_EQ_INT(p.v.mirror, 1);
    track_page_input(&p, DOWN);
    CHECK_EQ_INT(p.item, TP_RALLY);
    track_page_input(&p, ENTER);
    CHECK_EQ_INT(p.v.rally, 1);
    track_page_input(&p, DOWN);
    CHECK_EQ_INT(p.item, TP_TIME);
    track_page_input(&p, RIGHT);   /* day -> the next of the track's times in the toggle's order: day, dawn, dusk, night */
    CHECK_EQ_INT(p.v.light_time, LIGHT_TIME_DAWN);
    track_page_input(&p, RIGHT);   /* dusk is not one of its times */
    CHECK_EQ_INT(p.v.light_time, LIGHT_TIME_NIGHT);
    track_page_input(&p, RIGHT);
    CHECK_EQ_INT(p.v.light_time, LIGHT_TIME_DAY);   /* round */
    track_page_input(&p, LEFT);
    CHECK_EQ_INT(p.v.light_time, LIGHT_TIME_NIGHT);
    track_page_input(&p, DOWN);
    CHECK_EQ_INT(p.item, TP_WEATHER);
    track_page_input(&p, RIGHT);
    CHECK_EQ_INT(p.v.weather, WEATHER_FOG);
    track_page_input(&p, LEFT);
    track_page_input(&p, LEFT);
    CHECK_EQ_INT(p.v.weather, WEATHER_STORM);   /* clear, to the left: round to the storm */
}

static void test_a_time_the_track_does_not_take_is_stepped_to_one_it_does(void)
{
    TrackPage p = page(TRACK_TIME_DAY | TRACK_TIME_DUSK, 0, 1, 1);
    p.v.light_time = LIGHT_TIME_NIGHT;   /* a pick kept from another track */
    p.item = TP_TIME;
    track_page_input(&p, RIGHT);
    CHECK_EQ_INT(p.v.light_time, LIGHT_TIME_DAY);
    p.v.light_time = LIGHT_TIME_NIGHT;
    track_page_input(&p, LEFT);
    CHECK_EQ_INT(p.v.light_time, LIGHT_TIME_DUSK);
    CHECK_EQ_INT(track_page_time_shown(&p.offer, LIGHT_TIME_NIGHT), LIGHT_TIME_DAY);   /* what the page shows of a time it does not take */
    CHECK_EQ_INT(track_page_time_shown(&p.offer, LIGHT_TIME_DUSK), LIGHT_TIME_DUSK);
}

static void test_the_rows_and_the_bottom_row(void)
{
    TrackPage p = page(TRACK_TIME_DAY, 0, 1, 1);   /* Mirror, Weather */
    track_page_input(&p, DOWN);
    CHECK_EQ_INT(p.item, TP_WEATHER);   /* Rally and Time are passed over */
    track_page_input(&p, DOWN);
    CHECK_EQ_INT(p.item, TP_DONE);
    track_page_input(&p, RIGHT);
    CHECK_EQ_INT(p.item, TP_PREVIOUS);
    track_page_input(&p, RIGHT);
    CHECK_EQ_INT(p.item, TP_NEXT);
    track_page_input(&p, RIGHT);
    CHECK_EQ_INT(p.item, TP_BEST);
    track_page_input(&p, RIGHT);
    CHECK_EQ_INT(p.item, TP_DONE);   /* round */
    track_page_input(&p, LEFT);
    CHECK_EQ_INT(p.item, TP_BEST);
    track_page_input(&p, UP);
    CHECK_EQ_INT(p.item, TP_WEATHER);   /* up from the bottom row: the last row */
    track_page_input(&p, UP);
    track_page_input(&p, UP);
    CHECK_EQ_INT(p.item, TP_DONE);   /* up from the first row: round to DONE */
    CHECK_EQ_INT(track_page_input(&p, ENTER), TRACK_PAGE_LEAVE);
    CHECK_EQ_INT(track_page_input(&p, ESC), TRACK_PAGE_LEAVE);
    p.item = TP_PREVIOUS;
    CHECK_EQ_INT(track_page_input(&p, ENTER), TRACK_PAGE_GO_PREVIOUS);
    p.item = TP_NEXT;
    CHECK_EQ_INT(track_page_input(&p, ENTER), TRACK_PAGE_GO_NEXT);
    p.item = TP_BEST;
    CHECK_EQ_INT(track_page_input(&p, ENTER), TRACK_PAGE_BEST);
    CHECK_EQ_INT(track_page_input(&p, 'x'), TRACK_PAGE_STAY);   /* any other key does nothing */
    CHECK_EQ_INT(p.item, TP_BEST);
}

static void test_what_a_click_hits(void)
{
    const TrackPage p = page(TRACK_TIME_DAY | TRACK_TIME_NIGHT, 1, 1, 1);
    CHECK_EQ_INT(track_page_hit(&p, 30, 455), TP_DONE);
    CHECK_EQ_INT(track_page_hit(&p, 230, 455), TP_PREVIOUS);
    CHECK_EQ_INT(track_page_hit(&p, 390, 455), TP_NEXT);
    CHECK_EQ_INT(track_page_hit(&p, 560, 455), TP_BEST);
    CHECK_EQ_INT(track_page_hit(&p, 100, 304), TP_MIRROR);
    CHECK_EQ_INT(track_page_hit(&p, 100, 328), TP_RALLY);
    CHECK_EQ_INT(track_page_hit(&p, 100, 400), TP_TIME);
    CHECK_EQ_INT(track_page_hit(&p, 100, 424), TP_WEATHER);
    CHECK_EQ_INT(track_page_hit(&p, 450, 200), -1);   /* the panel */
    CHECK_EQ_INT(track_page_hit(&p, 100, 250), -1);   /* nothing there */
    const TrackPage q = page(TRACK_TIME_DAY, 0, 1, 1);   /* no Rally, no Time: the Mirror row sits where the last shown one stood (the rows stack up) */
    CHECK_EQ_INT(track_page_hit(&q, 100, 328), TP_MIRROR);
    CHECK_EQ_INT(track_page_hit(&q, 100, 400), -1);
    CHECK_EQ_INT(track_page_hit(&q, 100, 424), TP_WEATHER);
}

int main(void)
{
    test_the_rows_that_apply();
    test_the_values_the_keys_change();
    test_a_time_the_track_does_not_take_is_stepped_to_one_it_does();
    test_the_rows_and_the_bottom_row();
    test_what_a_click_hits();
    return TEST_REPORT();
}
