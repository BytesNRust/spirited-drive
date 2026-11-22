/* Sheets (dep98): several Race photos of a car in one labelled picture, so a mask is judged day and night, or the same car under other conditions, at a
 * glance. This file is the pure part: the cells of a sheet, the photos they get, and the grid they are laid out in (a label under each, in a built-in 5x7 font;
 * the editor has no other way to put text in a picture). race_sheet_run.h drives the photo job through the cells. A cell is a car, a project folder, a track and
 * a time of day; a later kind of cell (the cabin) adds fields here, not another type. */
#ifndef TNFS_TRACK_EDITOR_RACE_SHEET_H
#define TNFS_TRACK_EDITOR_RACE_SHEET_H

#include "car_export.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SHEET_CELLS = 16,
    SHEET_LABEL = 32,
    SHEET_NOTE = 160,
    SHEET_CELL_W = 640, SHEET_CELL_H = 480,   /* a cell's picture: a photo of 1280 x 960 (render scale 2) is halved to this, one of 640 x 480 is as it is */
    SHEET_LABEL_H = 20                        /* the band under a cell that holds its label */
};

typedef enum { SHEET_PENDING = 0, SHEET_DONE, SHEET_FAILED } SheetState;

typedef struct {
    char label[SHEET_LABEL];       /* under the cell: "Day", "Night", "COPMUST"... (any case, shown in capitals) */
    const ExportCar *car;
    char project[EXPORT_PATH];     /* the project folder that holds the car's masks */
    char track[8];                 /* a stem of car_race's track table */
    int night;
    SheetState state;
    char note[SHEET_NOTE];         /* a failed cell's reason (the job's summary) */
    uint8_t *rgba;                 /* a done cell's photo, SHEET_CELL_W x SHEET_CELL_H, RGBA */
} SheetCell;

typedef struct {
    int count, cols;
    SheetCell cell[SHEET_CELLS];
} RaceSheet;

/* an empty sheet laid out in `cols` columns (1..4) */
void sheet_init(RaceSheet *s, int cols);
void sheet_free(RaceSheet *s);
/* a pending cell; its index, or -1 when the sheet is full. The strings are cut to fit. */
int sheet_add(RaceSheet *s, const char *label, const ExportCar *car, const char *project, const char *track, int night);
/* cell `i` got its photo (RGBA, 1280 x 960 or 640 x 480: halved or copied into the cell); 0 for another size, a cell out of range or out of memory (the cell is failed then) */
int sheet_set_photo(RaceSheet *s, int i, const uint8_t *rgba, int w, int h);
/* cell `i` could not be photographed, `note` says why */
void sheet_set_failed(RaceSheet *s, int i, const char *note);
/* the first pending cell, -1 when there is none; how many cells are done or failed */
int sheet_next(const RaceSheet *s);
int sheet_finished_count(const RaceSheet *s);
/* every cell done or failed; false for an empty sheet */
int sheet_complete(const RaceSheet *s);
/* the picture: a cell's photo (grey with NO PHOTO when failed, empty while pending) and its label under it; `rgba` malloc'd; 0 with `why` for an empty sheet */
int sheet_compose(const RaceSheet *s, uint8_t **rgba, int *w, int *h, char *why, size_t why_cap);
/* the picture as a PNG file; 0 with `why` */
int sheet_write_png(const RaceSheet *s, const char *path, char *why, size_t why_cap);

#endif
