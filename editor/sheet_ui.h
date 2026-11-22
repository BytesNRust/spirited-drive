// The Race window's sheet row and picture size (dep98, app_sheet.h).
#ifndef TNFS_TRACK_EDITOR_SHEET_UI_H
#define TNFS_TRACK_EDITOR_SHEET_UI_H

#include "imgui.h"

extern "C" {
#include "app.h"
}

// «Sheet» and «Save sheet» with the progress and a «(?)» that says what each does; `busy` is a run in progress (they are dimmed)
void sheet_row(App *a, bool busy);

// the composed picture's size in pixels (the sheet's cells in their grid), for the Race window to fit it
ImVec2 sheet_size(const App *a);

#endif
