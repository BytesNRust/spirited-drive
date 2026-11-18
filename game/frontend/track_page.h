/* port (dep59 step 5, T7d): the page an added track's card opens in Control Central, in the look of the CD's track page (the sepia pages, the
 * title on top, the rows at the left, DONE / Road/track / Best times along the bottom) with the track's card, name, author, length and
 * description in the big box. This is its state and its keys, with no drawing and no files (track_page_glue.c draws it).
 * The rows are the settings the race reads (Mirror, Rally, the time of day, the weather), shown only where they apply to this track:
 * Rally with the track's rally art (NTRACKFM/<STEM>n_R01.FAM), the time of day when it takes more than one (times=) and the light effects
 * are on, the weather where the weather row is on. The values are the pick the page opens with; the glue writes them back as it closes. */
#ifndef TNFS_TRACK_PAGE_H
#define TNFS_TRACK_PAGE_H

#include <stdint.h>

enum { TP_MIRROR, TP_RALLY, TP_TIME, TP_WEATHER, TP_DONE, TP_PREVIOUS, TP_NEXT, TP_BEST, TP_ITEMS };
/* the keys the page takes: the front end's (showcase.c's) */
enum { TRACK_PAGE_KEY_ENTER = 0x0d, TRACK_PAGE_KEY_ESCAPE = 0x1b, TRACK_PAGE_KEY_UP = 0x4800, TRACK_PAGE_KEY_LEFT = 0x4b00,
       TRACK_PAGE_KEY_RIGHT = 0x4d00, TRACK_PAGE_KEY_DOWN = 0x5000 };
/* what track_page_input asks for */
enum { TRACK_PAGE_STAY = 0, TRACK_PAGE_LEAVE = 1, TRACK_PAGE_GO_PREVIOUS = 2, TRACK_PAGE_GO_NEXT = 3, TRACK_PAGE_BEST = 4, TRACK_PAGE_CANCEL = 5 };   /* Escape: the track screen's, the changes dropped */

/* what applies to this track */
typedef struct {
    int times;         /* the track's times of day, TRACK_TIME_* bits (track_piece.h) */
    int rally;         /* 1 when it has rally art: the Rally row is shown */
    int light_effects; /* 1 when the port's light effects are on: the time-of-day row is shown (a track of several times) */
    int weather;       /* 1 when the weather row is on */
    int no_mirror;     /* 1 when the mirror is not unlocked yet: no Mirror row */
} TrackPageOffer;

/* the pick: Mirror and Rally 0/1, the time of day a LIGHT_TIME_* (port_settings.h), the weather a WEATHER_* (weather.h) */
typedef struct {
    int mirror, rally, light_time, weather;
} TrackPageValues;

typedef struct {
    int item;   /* TP_* */
    TrackPageValues v;
    TrackPageOffer offer;
} TrackPage;

/* the page opened on its first row (DONE when no row applies) */
void track_page_open(TrackPage *p, const TrackPageOffer *offer, const TrackPageValues *values);
/* 1 when the item is on the page for this track (the bottom row always is) */
int track_page_shown(const TrackPage *p, int item);
/* the time of day the page shows for `light_time`: itself when the track takes it, else the first of its times (day first) */
int track_page_time_shown(const TrackPageOffer *offer, int light_time);
/* a key: the state changed; TRACK_PAGE_STAY, or what is asked for (Done leaves, Escape cancels) */
int track_page_input(TrackPage *p, int key);
/* the item shown at (x, y), the page's 640 x 480, or -1 */
int track_page_hit(const TrackPage *p, int x, int y);
/* the y its words are drawn at on the page (Mirror, Rally, Time, Weather): the switch rows stack up from where the CD's variant name stands, the
 * last shown lowest, so a row that does not apply leaves no gap; -1 when the row is not shown */
int track_page_row_y(const TrackPage *p, int item);

/* the description's first visible line `frame` frames (60 a second) after the page opened: of `total` lines `visible` show at once; one
 * more line every TRACK_PAGE_SCROLL_FRAMES, a step's pause first and two at the end, then round to the first. 0 when all fit. */
enum { TRACK_PAGE_SCROLL_FRAMES = 180 };
int track_page_scroll_line(int frame, int total, int visible);

#endif
