/* Physics from in the car's card panel (dep102): a row with the car to copy and a Copy... button, and the window that asks before it overwrites the car's specs.tsv and pdn.tsv. */
#ifndef TNFS_EDITOR_PHYSICS_POPUP_H
#define TNFS_EDITOR_PHYSICS_POPUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app.h"

#ifdef __cplusplus
}
#endif

void physics_row(App *a);     /* inside the card panel, under the mod.ini table, beside Sound from */
void physics_popup(App *a);   /* once a frame with the other windows */

#endif
