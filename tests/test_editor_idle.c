/* The window at rest (editor/idle.c): drawn at the frame rate while something moves, and only every IDLE_BEAT seconds once nothing has for IDLE_AFTER seconds. */
#include "idle.h"
#include "test.h"

#include <stdlib.h>

int main(void)
{
    Idle s = { 0, 0, 0 };
    CHECK(idle_draw(&s, 10.0, 0, 0));   // the first turn is drawn
    CHECK(!idle_resting(&s, 10.0));
    int drawn = 0;
    for (int i = 1; i <= 100; i++) drawn += idle_draw(&s, 10.0 + i * 0.016, 0, 0);   // 1.6 s of frames with no event: inside IDLE_AFTER, every one drawn
    CHECK(drawn == 100);
    // at rest: from 12 s on, 10 s of turns every 16 ms are drawn only every IDLE_BEAT
    drawn = 0;
    for (int i = 0; i < 625; i++) drawn += idle_draw(&s, 12.0 + i * 0.016, 0, 0);
    CHECK(idle_resting(&s, 22.0));
    CHECK(drawn >= 19 && drawn <= 21);   // about 20 in 10 s, not 625
    // an event wakes it: drawn at once and for IDLE_AFTER more seconds
    CHECK(idle_draw(&s, 30.0, 1, 0));
    drawn = 0;
    for (int i = 1; i <= 100; i++) drawn += idle_draw(&s, 30.0 + i * 0.016, 0, 0);
    CHECK(drawn == 100 && !idle_resting(&s, 31.6));
    // a busy job (an animation, the test drive) keeps every frame drawn however long it lasts
    drawn = 0;
    for (int i = 0; i < 1000; i++) drawn += idle_draw(&s, 40.0 + i * 0.016, 0, 1);
    CHECK(drawn == 1000 && !idle_resting(&s, 55.0));
    // what moves by itself is named in app_animating: the Model's Auto-rotate, the Slow wheels (not in a headless run, which fixes them), the police lights; only in a car
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a != NULL) {
        CHECK(!app_animating(a));
        a->kind = APP_KIND_CAR;
        CHECK(!app_animating(a));
        a->model_auto_rotate = 1;
        CHECK(app_animating(a));
        a->model_auto_rotate = 0, a->model_wheels = 1;
        CHECK(app_animating(a));
        a->model_wheels_fixed = 1;
        CHECK(!app_animating(a));
        a->model_wheels = 0, a->night.police = 1;
        CHECK(app_animating(a));
        a->kind = APP_KIND_TRACK;
        CHECK(!app_animating(a));
        free(a);
    }
    return TEST_REPORT();
}
