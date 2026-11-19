/* port (dep59 showcase, owner 2026-11-18): the RECORDS screen of a car's page (Video's tile on the added car's page, a face of it on the CD's): a table of
 * every track of the edition with that car's records there, a few to a page (car_records.h gives the figures of each). The screen is a
 * screen of its own, laid out as Best Times is (car_records_screen.h): this file holds the table (its rows, its pages) and how it is drawn. */
#ifndef TNFS_CAR_RECORDS_VIEW_H
#define TNFS_CAR_RECORDS_VIEW_H

#include "game/frontend/car_records.h"
#include "game/frontend/track_list.h"

#include <stddef.h>
#include <stdint.h>

enum {
    CAR_RECORDS_ROWS = 10,                   /* tracks to a page: Best Times' ten rows */
    CAR_RECORDS_MAX = TRACK_LIST_MAX,        /* tracks in all */
    CAR_RECORDS_NAME = 40,
    /* where it is drawn (a 640 x 480 screen): Best Times' rows' places (best_times.c ROW_Y, ROW_STEP) and the title band's end */
    CAR_RECORDS_BAND = 58, CAR_RECORDS_HEAD_Y = 96, CAR_RECORDS_ROW0 = 0x73, CAR_RECORDS_STEP = 0x16, CAR_RECORDS_LABEL_Y = 66, CAR_RECORDS_HEAD_SHADOW = 2 /* as the track names' shadow: +2, +2 */, CAR_RECORDS_HEAD_ROWS = 12, CAR_RECORDS_NOTE_Y = 400,
    /* DONE, measured on the CD's showcase page (capitals 12 rows) and put so that its last row (456) is the page indicator's (page_arrows.h:
     * the count's last row on the original's DONE); its ink from x 10 */
    CAR_RECORDS_FOOT_CAP = 445, CAR_RECORDS_FOOT_CAP_H = 12, CAR_RECORDS_DONE_X = 10, CAR_RECORDS_FOOT_TOP = 435   /* a click from this row down is the footer's */
};

/* the palette's indices the screen draws in (the page's own: car_records_screen.c finds the nearest of each) */
typedef struct {
    int cream, yellow, red, shadow;
    int head_shadow;   /* the red heads' shadow in pixels (down and right): 0 flat, CAR_RECORDS_HEAD_SHADOW on a reddish page (car_records_reddish) */
} CarRecordsInk;

/* owner point 27: the page's tint is a hue, never a loud colour. The tinted background (RGB, `pixels` of them) is limited to what the car's own page has:
 * its brightness (the mean of each pixel's greatest channel) to CAR_RECORDS_V_MAX percent, and each pixel's saturation to CAR_RECORDS_S_MAX percent.
 * The figures are the 512TR's original page (a mean brightness of 60% and a saturation of at most 80% over its background). Less than that is left as it is */
enum { CAR_RECORDS_V_MAX = 58, CAR_RECORDS_S_MAX = 80 };
void car_records_limit_tint(uint8_t *rgb, size_t pixels);

/* owner point 23: the red heads carry a dark shadow only on a reddish page. The rule is the hue and saturation of the page's mean colour behind the heads:
 * red when the hue is within 15 degrees of red and the saturation (max - min over max) is 50% or more. 1 when `rgb` is that */
int car_records_reddish(int r, int g, int b);

/* a flat-coloured text (the main face's mask at num/den, a coverage from half up) at (x, y) with a shadow `shadow_px` down and right (0 none) */
void car_records_ink_text(const CarRecordsInk *ink, const char *text, int x, int y, int num, int den, int colour, int shadow_px);

typedef struct {
    char name[CAR_RECORDS_NAME];
    CarRecord record;
} CarRecordsRow;

typedef struct {
    char car[40];                /* the car's id */
    CarRecordsRow row[CAR_RECORDS_MAX];
    int count, page;
} CarRecordsView;

/* the table of `car` over `tracks` (the edition's, as the track arrows walk them): a stock track by its number and name, Lunar Springs by its id,
 * an added track by its id and name */
void car_records_view_make(CarRecordsView *v, const char *car, const TrackList *tracks);
/* how many pages (at least 1) and the page stepped by `dir` (1 on, -1 back), round */
int car_records_view_pages(const CarRecordsView *v);
void car_records_view_step(CarRecordsView *v, int dir);

/* the page drawn with the text styles of the screen's palette into the back buffer: the label "Best Times", the column heads in red, the rows and the note under them.
 * `kmh` the speed unit as the menus show it (resource_kmh) */
void car_records_view_draw(const CarRecordsView *v, int kmh, const CarRecordsInk *ink);

/* the footer's DONE at the left, in the footer's big letters; the page indicator is page_arrows.h's, drawn by the screen */
void car_records_view_footer(const CarRecordsView *v, const CarRecordsInk *ink);

#endif
