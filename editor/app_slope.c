#include "app_slope.h"

#include "app_select.h"
#include "slope_block.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_slope_defaults(App *a)
{
    if (a->slope.ready) return;
    a->slope.ready = 1;
    a->slope.grade_pct = 6.0, a->slope.rise_m = 5.0, a->slope.ease_in_m = 40.0, a->slope.ease_out_m = 40.0, a->slope.keep_after = 1;
}

static double kept_height(double h)
{
    double want = round(h / TRACE_HEIGHT_STEP_M) * TRACE_HEIGHT_STEP_M;
    return want > TRACE_HEIGHT_MAX_M ? TRACE_HEIGHT_MAX_M : want < -TRACE_HEIGHT_MAX_M ? -TRACE_HEIGHT_MAX_M : want;
}

void app_slope_plan_free(SlopePlan *plan)
{
    free(plan->points);
    plan->points = NULL;
}

static void refuse(SlopePlan *plan, const char *why)
{
    plan->ok = 0;
    snprintf(plan->text, sizeof plan->text, "%s", why);
}

void app_slope_plan(App *a, SlopePlan *plan)
{
    memset(plan, 0, sizeof *plan);
    plan->first = plan->last = -1;
    if (a->kind != APP_KIND_TRACK) {
        refuse(plan, "no track");
        return;
    }
    app_slope_defaults(a);
    app_sel_sync(a);
    const int first = sel_first(&a->sel), last = sel_last(&a->sel), picked = sel_count(&a->sel);
    if (picked < 2 || last - first + 1 != picked) {
        refuse(plan, "select a run of points without gaps, two or more (Shift+click its last point)");
        return;
    }
    plan->first = first, plan->last = last;
    const int count = a->trace.count, n = last - first + 1;
    double *metres = malloc(sizeof *metres * (size_t)count), *dist = malloc(sizeof *dist * (size_t)n), *heights = malloc(sizeof *heights * (size_t)n);
    TracePoint *points = malloc(sizeof *points * (size_t)count);
    if (metres == NULL || dist == NULL || heights == NULL || points == NULL) {
        free(metres), free(dist), free(heights), free(points);
        refuse(plan, "out of memory");
        return;
    }
    memcpy(points, a->trace.points, sizeof *points * (size_t)count);
    track_trace_point_distances(a->trace.points, count, metres);
    for (int i = 0; i < n; ++i) dist[i] = metres[first + i] - metres[first];
    plan->length_m = dist[n - 1];
    const double before = first > 0 ? track_trace_segment_grade(a->trace.points, metres, count, first - 1, TRACE_GRADE_PERCENT)
                                    : track_trace_segment_grade(a->trace.points, metres, count, first, TRACE_GRADE_PERCENT);
    const double after = last + 1 < count ? track_trace_segment_grade(a->trace.points, metres, count, last, TRACE_GRADE_PERCENT)
                                          : track_trace_segment_grade(a->trace.points, metres, count, last - 1, TRACE_GRADE_PERCENT);
    double grade = 0;
    const int by_rise = a->slope.by_rise != 0;
    if (!slope_block(dist, n, a->trace.points[first].h, before, after, by_rise, by_rise ? a->slope.rise_m : a->slope.grade_pct, a->slope.ease_in_m, a->slope.ease_out_m, heights, &grade)) {
        free(metres), free(dist), free(heights), free(points);
        refuse(plan, "the fields are out of range: the easings must fit in the run (together no longer than it), the grade within 100 %, the rise within 500 m");
        return;
    }
    const double old_last = a->trace.points[last].h;
    for (int i = 0; i < n; ++i) points[first + i].h = kept_height(heights[i]);
    if (a->slope.keep_after) {
        const double delta = points[last].h - old_last;
        for (int i = last + 1; i < count; ++i) points[i].h = kept_height(points[i].h + delta);
    }
    plan->grade_pct = grade;
    plan->rise_m = points[last].h - points[first].h;
    double steepest = 0;
    for (int seg = first - 1 < 0 ? 0 : first - 1; seg < last + (a->slope.keep_after || last + 1 >= count ? 0 : 1); ++seg) {
        const double g = fabs(track_trace_segment_grade(points, metres, count, seg, TRACE_GRADE_PERCENT));
        if (seg >= first && g > steepest) steepest = g;
        if (g > TRACK_GEN_GRADE_MAX_PERCENT) plan->red++;
    }
    plan->steepest_pct = steepest;
    free(metres), free(dist), free(heights);
    plan->points = points;
    snprintf(plan->text, sizeof plan->text, "%s %.1f m over %.0f m, steepest %.0f %%", plan->rise_m >= 0 ? "rises" : "falls", fabs(plan->rise_m), plan->length_m, steepest);
    if (plan->red > 0) {
        const size_t used = strlen(plan->text);
        snprintf(plan->text + used, sizeof plan->text - used, "; %d segment%s over %.0f %%: not made", plan->red, plan->red == 1 ? "" : "s", TRACK_GEN_GRADE_MAX_PERCENT);
        return;
    }
    plan->ok = 1;
}

int app_slope_accept(App *a)
{
    SlopePlan plan;
    app_slope_plan(a, &plan);
    if (!plan.ok) {
        snprintf(a->status, sizeof a->status, "slope: %s", plan.text);
        app_slope_plan_free(&plan);
        return 0;
    }
    for (int i = 0; i < a->trace.count; ++i) a->trace.points[i].h = plan.points[i].h;
    a->dirty = 1;
    snprintf(a->status, sizeof a->status, "slope made: %s", plan.text);
    app_slope_plan_free(&plan);
    return 1;
}
