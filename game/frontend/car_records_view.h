/* port (dep59 showcase, owner 2026-11-18): the RECORDS screen of a car's page (Video's tile on the added car's page, a face of it on the CD's): a table of
 * every track of the edition with that car's records there, a few to a page (car_records.h gives the figures of each). The screen is a
 * screen of its own, laid out as Best Times is (car_records_screen.h): this file holds the table (its rows, its pages) and how it is drawn. */
#ifndef TNFS_CAR_RECORDS_VIEW_H
#define TNFS_CAR_RECORDS_VIEW_H

#include "game/frontend/car_records.h"
#include "game/frontend/track_list.h"

enum {
    CAR_RECORDS_ROWS = 10,                   /* tracks to a page: Best Times' ten rows */
    CAR_RECORDS_MAX = TRACK_LIST_MAX,        /* tracks in all */
    CAR_RECORDS_NAME = 40,
    /* where it is drawn, on Best Times' art (a 640 x 480 screen): its rows' places (best_times.c ROW_Y, ROW_STEP), the page selector over its footer */
    CAR_RECORDS_HEAD_Y = 96, CAR_RECORDS_ROW0 = 0x73, CAR_RECORDS_STEP = 0x16, CAR_RECORDS_LABEL_Y = 66,
    CAR_RECORDS_PAGER_Y = 428, CAR_RECORDS_PAGER_X = 311
};

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

/* the page drawn with the text styles of the screen's palette into the selected picture: the label "RECORDS", the column heads in red, the rows,
 * the page selector. `kmh` the speed unit as the menus show it (resource_kmh) */
void car_records_view_draw(const CarRecordsView *v, int kmh);

/* what a click at (x, y) on the page means: -1 the selector's left half, 1 its right half, 0 nothing */
int car_records_view_hit(const CarRecordsView *v, int x, int y);

#endif
