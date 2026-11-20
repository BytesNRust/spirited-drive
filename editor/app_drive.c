/* Test drive of the open track (app.h): the checks the Drive window's button makes, then the build and the game's run (car_race_start_drive). Its own file:
 * app.c is the session's. */
#include "app.h"

#include "build.h"

#include <stdio.h>

/* the road as the game will take it for a test drive: 0 with the status saying why (the first red problem, as the Problems window says it) */
static int road_drivable(App *a)
{
    char text[APP_MSG];
    if (!a->road_made) return snprintf(a->status, sizeof a->status, "test drive: %s", a->road_text), 0;
    for (int i = 0; i < a->problem_count; ++i)
        if (a->problems[i].red) {
            track_trace_problem_text(&a->problems[i], text, sizeof text);
            return snprintf(a->status, sizeof a->status, "test drive: fix the red problems first: %s", text), 0;
        }
    if (a->stage_short) return snprintf(a->status, sizeof a->status, "test drive: %s", a->stage_text), 0;
    return 1;
}

int app_test_drive(App *a)
{
    char why[APP_MSG];
    if (a->kind != APP_KIND_TRACK) return snprintf(a->status, sizeof a->status, "test drive: open a track first"), 0;
    if (car_race_busy(&a->race)) return snprintf(a->status, sizeof a->status, "test drive: the game is running already"), 0;
    if (install_job_busy(&a->install)) return snprintf(a->status, sizeof a->status, "test drive: an install is running: wait for it to end"), 0;
    app_recompute(a);   /* the road as it is now, not as the last frame had it */
    if (!road_drivable(a)) return 0;
    if (!app_check_game(a)) return 0;   /* the status says where it was looked for and what to do, as Install's */
    const ExportCar *car = car_race_menu_car(a->drive_pick);
    if (car == NULL) return snprintf(a->status, sizeof a->status, "test drive: pick a car"), 0;
    if (!car_race_piece_id_ok(a->project.id)) return snprintf(a->status, sizeof a->status, "test drive: the id %s has more than letters, digits, - and _", a->project.id), 0;
    if (a->export_src.cd[0] == '\0') return snprintf(a->status, sizeof a->status, "test drive: the game's files were not found (start the editor with --cd DIR)"), 0;
    if (!app_build(a)) return 0;
    if (!car_race_start_drive(&a->race, a->game, a->export_src.cd, a->zip, a->project.id, car, &a->race_look, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "test drive: %s", why);
        return 0;
    }
    snprintf(a->status, sizeof a->status, "test drive: installing %s, then the game runs on it: close its window or Stop to end it", a->project.id);
    return 1;
}
