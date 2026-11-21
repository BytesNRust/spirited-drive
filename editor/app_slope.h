/* The track editor's slope tool (dep95 step 10): over the selected run of control points (two or more, no gaps) the heights for a grade or a rise (slope_block.h),
 * with the road after the run kept on it (moved by the run's last point's change) or left where it is. The plan is what Accept would make, with its words; Accept
 * writes it as one step to undo and refuses a result with a segment over the generator's grade limit, as Add curve refuses a bad count. */
#ifndef TNFS_TRACK_EDITOR_APP_SLOPE_H
#define TNFS_TRACK_EDITOR_APP_SLOPE_H

#include "app.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int ok;                       /* the plan can be accepted */
    int first, last;              /* the run's points (set whenever a run is selected) */
    double rise_m, length_m;      /* the run's last height less its first, and its length in metres */
    double grade_pct;             /* the grade between the easings */
    double steepest_pct;          /* the steepest segment of the run (its size) */
    int red;                      /* segments over the generator's grade limit, in the run and the one before it */
    TracePoint *points;           /* the trace as it would be (a->trace.count points), NULL when there is no plan; freed by app_slope_plan_free */
    char text[200];               /* "rises 4.2 m over 180 m, steepest 9 %" or the reason there is no plan */
} SlopePlan;

/* the fields' defaults (6 %, 5 m, eased 40 m each end, the road after kept) once, while they are all 0 */
void app_slope_defaults(App *a);
/* what Accept would make; always fills `text`; the plan's points are the caller's to free (app_slope_plan_free) */
void app_slope_plan(App *a, SlopePlan *plan);
void app_slope_plan_free(SlopePlan *plan);
/* the heights written: one step to undo, the selection kept, the status says what it made; 1 when done, 0 with the reason in the status */
int app_slope_accept(App *a);

#ifdef __cplusplus
}
#endif

#endif
