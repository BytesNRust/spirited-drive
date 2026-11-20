/* Sheets in the app (dep98): the Race window's Sheet and Traffic sheet, run one photo after another through the Race photo job (car_race.h, App.race) and put in
 * App.sheet (race_sheet.h). */
#ifndef TNFS_TRACK_EDITOR_APP_SHEET_H
#define TNFS_TRACK_EDITOR_APP_SHEET_H

#include "app.h"

/* the car's sheet (Day and Night of the open car at the Race window's track, as the Race window has its look) or, with `traffic`, the sheet of every traffic car and the
 * police car at the Race window's time of day, each from its project folder beside the open one (a car with none is a cell that says so). 1 when it started, 0 with the
 * status line saying why (no car, no game, a sheet already running, the Race job busy) */
int app_sheet_start(App *a, int traffic);
/* every frame, after app_race_poll: takes a finished photo into its cell and starts the next one; 1 while the sheet runs */
int app_sheet_poll(App *a);
/* ends the running sheet: the photo in progress is stopped, the cells not yet taken say so */
void app_sheet_stop(App *a);
int app_sheet_busy(const App *a);
/* writes the sheet as a PNG in the open project's sheets/ folder and asks for the folder to be shown (App.reveal); 1 when written */
int app_sheet_save(App *a);
/* "Day:done Night:failed ..." for --steps print-sheet */
void app_sheet_describe(const App *a, char *out, size_t cap);

#endif
