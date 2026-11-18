/* port (dep59 step 5, T7d): see track_page.h. */
#include "game/frontend/track_page.h"

#include "formats/track_piece.h"
#include "game/frontend/track_select.h"
#include "game/sys/port_settings.h"

enum {
    SWITCH_LAST_Y = 316, SWITCH_PITCH = 24,   /* the CD's page: the variant's name stood at 0x13c, the switch rows a row apart above it */
    TIME_Y0 = 388, TIME_Y1 = 416, WEATHER_Y0 = 417, WEATHER_Y1 = 434,   /* the rows' hit boxes (the CD's) */
    TIME_TEXT_Y = 386, WEATHER_TEXT_Y = 410,                              /* where their words are drawn: the CD's rows */
    ROWS_X1 = 300,                            /* the rows end where the big box begins */
    BOTTOM_Y0 = 435, BOTTOM_Y1 = 479,
    DONE_X1 = 130, PREVIOUS_X0 = 200, PREVIOUS_X1 = 250, NEXT_X0 = 370, NEXT_X1 = 420, BEST_X0 = 480, BEST_X1 = 639
};

static int time_bit(int light_time)
{
    return light_time == LIGHT_TIME_DAWN ? TRACK_TIME_DAWN : light_time == LIGHT_TIME_DUSK ? TRACK_TIME_DUSK
                                                                                            : light_time == LIGHT_TIME_NIGHT ? TRACK_TIME_NIGHT : TRACK_TIME_DAY;
}

static int time_taken(const TrackPageOffer *offer, int light_time) { return (offer->times & time_bit(light_time)) != 0; }

static int times_count(const TrackPageOffer *offer)
{
    int n = 0;
    for (int t = 0; t < 4; t++) n += time_taken(offer, t);
    return n;
}

int track_page_shown(const TrackPage *p, int item)
{
    switch (item) {
    case TP_MIRROR: return p->offer.no_mirror == 0;
    case TP_RALLY: return p->offer.rally != 0;
    case TP_TIME: return p->offer.light_effects != 0 && times_count(&p->offer) > 1;
    case TP_WEATHER: return p->offer.weather != 0;
    default: return item >= TP_DONE && item < TP_ITEMS;
    }
}

int track_page_time_shown(const TrackPageOffer *offer, int light_time)
{
    if (time_taken(offer, light_time)) return light_time;
    for (int t = 0; t < 4; t++)
        if (time_taken(offer, t)) return t;
    return LIGHT_TIME_DAY;
}

void track_page_open(TrackPage *p, const TrackPageOffer *offer, const TrackPageValues *values)
{
    p->offer = *offer;
    p->v = *values;
    p->item = TP_DONE;
    for (int i = TP_WEATHER; i >= TP_MIRROR; i--)
        if (track_page_shown(p, i)) p->item = i;
}

/* the row `step` (1 down, -1 up) from the row `item`, DONE past the last and the first */
static int row_step(const TrackPage *p, int item, int step)
{
    for (int i = item + step; i >= TP_MIRROR && i <= TP_WEATHER; i += step)
        if (track_page_shown(p, i)) return i;
    return TP_DONE;
}

/* the first row from the top (step 1) or the bottom (step -1) that is shown, DONE when none */
static int row_edge(const TrackPage *p, int step)
{
    return row_step(p, step > 0 ? TP_MIRROR - 1 : TP_WEATHER + 1, step);
}

static void value_step(TrackPage *p, int step)
{
    switch (p->item) {
    case TP_MIRROR: p->v.mirror = !p->v.mirror; break;
    case TP_RALLY: p->v.rally = !p->v.rally; break;
    case TP_TIME:
        for (int i = 0, t = p->v.light_time; i < 4; i++) {
            t = track_select_light_time_step(t, step);
            if (time_taken(&p->offer, t)) {
                p->v.light_time = t;
                break;
            }
        }
        break;
    case TP_WEATHER: p->v.weather = track_select_weather_step(p->v.weather, step); break;
    default: break;
    }
}

int track_page_input(TrackPage *p, int key)
{
    const int on_row = p->item <= TP_WEATHER;
    switch (key) {
    case TRACK_PAGE_KEY_ESCAPE: return TRACK_PAGE_LEAVE;
    case TRACK_PAGE_KEY_UP:
        p->item = on_row ? row_step(p, p->item, -1) : row_edge(p, -1);
        break;
    case TRACK_PAGE_KEY_DOWN:
        p->item = on_row ? row_step(p, p->item, 1) : row_edge(p, 1);
        break;
    case TRACK_PAGE_KEY_LEFT:
    case TRACK_PAGE_KEY_RIGHT: {
        const int step = key == TRACK_PAGE_KEY_RIGHT ? 1 : -1;
        if (on_row) value_step(p, step);
        else p->item = TP_DONE + ((p->item - TP_DONE + step + 4) % 4);
        break;
    }
    case TRACK_PAGE_KEY_ENTER:
        if (on_row) value_step(p, 1);
        else if (p->item == TP_DONE) return TRACK_PAGE_LEAVE;
        else return p->item == TP_PREVIOUS ? TRACK_PAGE_GO_PREVIOUS : p->item == TP_NEXT ? TRACK_PAGE_GO_NEXT : TRACK_PAGE_BEST;
        break;
    default: break;
    }
    return TRACK_PAGE_STAY;
}

int track_page_row_y(const TrackPage *p, int item)
{
    if (!track_page_shown(p, item)) return -1;
    if (item == TP_TIME) return TIME_TEXT_Y;
    if (item == TP_WEATHER) return WEATHER_TEXT_Y;
    const int below = item == TP_MIRROR && track_page_shown(p, TP_RALLY);
    return SWITCH_LAST_Y - (below ? SWITCH_PITCH : 0);
}

int track_page_hit(const TrackPage *p, int x, int y)
{
    if (x < 0 || y < 0) return -1;
    if (y >= BOTTOM_Y0 && y <= BOTTOM_Y1) {
        if (x <= DONE_X1) return TP_DONE;
        if (x >= PREVIOUS_X0 && x <= PREVIOUS_X1) return TP_PREVIOUS;
        if (x >= NEXT_X0 && x <= NEXT_X1) return TP_NEXT;
        return x >= BEST_X0 && x <= BEST_X1 ? TP_BEST : -1;
    }
    if (x > ROWS_X1) return -1;
    if (track_page_shown(p, TP_TIME) && y >= TIME_Y0 && y <= TIME_Y1) return TP_TIME;
    if (track_page_shown(p, TP_WEATHER) && y >= WEATHER_Y0 && y <= WEATHER_Y1) return TP_WEATHER;
    for (int item = TP_MIRROR; item <= TP_RALLY; item++) {
        const int top = track_page_row_y(p, item);   /* (the switch rows' words are at the top of their boxes) */
        if (top >= 0 && y >= top && y < top + SWITCH_PITCH) return item;
    }
    return -1;
}

int track_page_scroll_line(int frame, int total, int visible)
{
    if (visible <= 0 || total <= visible || frame < 0) return 0;
    const int moves = total - visible, period = moves + 3, step = (frame / TRACK_PAGE_SCROLL_FRAMES) % period;
    return step - 1 < 0 ? 0 : step - 1 > moves ? moves : step - 1;
}
