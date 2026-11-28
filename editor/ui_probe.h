/* Where the panels' widgets are, for the input script (steps.c ui-click, ui-at, ui-span): a panel names the widget it just drew (ui_probe) and the steps look the name up, then send
 * the mouse there as SDL events, so a test clicks what a person clicks. A name not drawn in the last frame is not found. */
#ifndef TNFS_TRACK_EDITOR_UI_PROBE_H
#define TNFS_TRACK_EDITOR_UI_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/* the rectangle of a drawn widget in window pixels; 1, or 0 when no widget of that name was drawn in the last frame */
int ui_probe_find(const char *name, float *x, float *y, float *w, float *h);
/* brings the window of that name to the front (a docked tab shows nothing of its panel until it is), for ui-focus */
void ui_probe_focus(const char *window);

/* The tests' sabotage (--steps same-ids): every row of the card's photo list (0) or colour list (1) takes one ID, which the sweep (scan) must report; -1 none */
void ui_probe_same_ids(int list);
int ui_probe_row_id(int list, int row);   /* what a row pushes as its ID: `row`, or 0 for the list under sabotage */

#ifdef __cplusplus
}

/* names the last item drawn: scope_name when a scope is set (ui_probe_scope("brightness0")), else name. Cheap: a copy of four floats. */
void ui_probe(const char *name);
void ui_probe_scope(const char *scope);   /* "" ends it */
void ui_probe_rect(const char *name, float x, float y, float w, float h);   /* a rectangle that is no item (a table's column border) */
#endif

#endif
