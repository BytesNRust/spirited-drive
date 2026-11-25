/* The Track light panel's tables (Tunnels, Lamps): the columns fill the panel's width, each keeps what its header and its cells need (the table scrolls across when the panel is
 * narrower than all of them), a drag of a header's border changes a width like a spreadsheet's, and the widths are kept with the layout (ImGui's table settings in Studio's layout
 * file, written when the mouse is let go: ui.cpp). The widths are weights of the columns, so a panel that is wider or narrower later keeps their proportions. */
#ifndef TNFS_TRACK_EDITOR_TABLE_FIT_H
#define TNFS_TRACK_EDITOR_TABLE_FIT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* for the tests (steps.c fit-report): one line a column of every table drawn in the last frame, `fit TABLE COLUMN width W need N` (W the width the column has, N what its header and cells need) */
void table_fit_report(FILE *out);

#ifdef __cplusplus
}

typedef struct {
    const char *label;   // the header ("" for a column with a button and no header text)
    float need;          // the pixels of content the column needs at least (its widest cell); less than the header's text: the header's text
} FitColumn;

/* BeginTable `id` with `count` columns (at most 16) that fill the panel, resizable, scrolling across when they do not fit. `height` as BeginTable's outer height. Same result as BeginTable: when
 * true, the caller draws the header row and the rows and ends it with table_fit_end(). */
bool table_fit_begin(const char *id, const FitColumn *columns, int count, float height);
void table_fit_end();

/* the width an item has in a cell: all of it (not -FLT_MIN, which a stretch column turns into a NaN size under UBSan) */
float table_fit_cell();
#endif

#endif
