// The Points panel's Add curve and Add straight rows (dep88): curve_panel.cpp.
#ifndef TNFS_TRACK_EDITOR_CURVE_PANEL_H
#define TNFS_TRACK_EDITOR_CURVE_PANEL_H

extern "C" {
#include "app.h"
}

// the rows in a folding block: folded to begin with when the panel's room left after them would be under `table_least` (the Points table's least height)
void curve_rows(App *a, float table_least);

#endif
