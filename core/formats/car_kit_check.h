/* What a car kit's author is told before building it (dep85 step 3). The kit is checked, not built: each rule names what is wrong and where, in
 * the author's words (an object of the OBJ, a file of the kit), so the Studio's Problems panel and `tnfs-studio car check KIT` print the same lines.
 * An error is what the game or the importer cannot take; a warning is what is probably a slip. The wheels are checked from the near model's
 * OBJ (Blender's frame: the car's front towards -Z, its right towards +X, y up), by shape alone, with no unit: the sizes are compared to each
 * other, never to a number of the game's. */
#ifndef TNFS_CAR_KIT_CHECK_H
#define TNFS_CAR_KIT_CHECK_H

#include "formats/obj_read.h"

#include <stddef.h>

typedef enum { KIT_WARNING, KIT_ERROR } KitSeverity;

typedef struct {
    KitSeverity severity;
    char id[28];       /* what rule: "wheel-missing", "wheel-twice", ... (stable; tests and the Problems panel key on it) */
    char text[300];    /* the message for the author */
} KitProblem;

typedef struct {
    KitProblem *item;
    size_t count, cap;
} KitProblems;

void kit_problems_init(KitProblems *p);
void kit_problems_free(KitProblems *p);
size_t kit_problems_errors(const KitProblems *p);

/* The four wheel objects of the near model's OBJ (names as the importer reads them: wheel_fl, wheel_fr, wheel_rl, wheel_rr, case aside, a
 * Blender number after a dot ignored). Appends to `out`:
 *   errors   wheel-missing (a front wheel), wheel-twice (two objects are one wheel), wheel-no-faces
 *   warnings wheel-missing-rear, wheel-not-round (height and depth differ by over 15 %), wheel-too-wide (wider than it is high),
 *            wheel-side (a left wheel right of the middle or the reverse), wheel-order (a front wheel behind a rear one),
 *            wheel-pair-size (a pair's two wheels differ by over 10 %), wheel-ground (the wheels' bottoms differ by over 10 % of a diameter)
 * Returns 0 only when memory ran out. */
int car_kit_check_wheels(const ObjModel *near_model, KitProblems *out);

#endif
