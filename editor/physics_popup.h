/* Physics from in the car's card panel (dep102): a row with the car to copy and a Copy... button, and the window that asks before it overwrites the car's specs.tsv and pdn.tsv. */
#ifndef TNFS_EDITOR_PHYSICS_POPUP_H
#define TNFS_EDITOR_PHYSICS_POPUP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "app.h"

#ifdef __cplusplus
}
#endif

/* A row of the card panel's mod.ini table, after Sound from (called between BeginTable and EndTable). */
void physics_row(App *a);
/* What the row's box says: a game car's own physics (its word, or its stem), else the car picked, else "not set". `buf` holds a stem when that is what is returned. */
#ifdef __cplusplus
extern "C"
#endif
const char *physics_row_text(const App *a, char *buf, size_t cap);
void physics_popup(App *a);   /* once a frame with the other windows */

#endif
