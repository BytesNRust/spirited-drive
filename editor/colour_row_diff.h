/* What Copy to game repo says about a car's row of colors in core/formats/car_paint.c (editor/app_car_copy.c): the row before and after are lines «Name #rrggbb, Name #rrggbb, ...»
 * (car_table_rewrite.h); this says how many colors there are before and after (the game's table row grows or shrinks) and which of them changed value, were added or were taken away.
 * No window, no files. */
#ifndef TNFS_TRACK_EDITOR_COLOUR_ROW_DIFF_H
#define TNFS_TRACK_EDITOR_COLOUR_ROW_DIFF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The lines (each indented two spaces, closed by a newline) written into out: «colors: 2 -> 8 (the game's table row goes to 8)» or «colors: 4 (the same number)», then «changed: Name #a -> #b; ...»
 * (a name that changed too: «Old #a -> New #b»), «added: ...» and «removed: ...» for the ones that have any. Returns the length written (the text is closed, cut at `cap`). */
size_t colour_row_diff(const char *before, const char *after, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
