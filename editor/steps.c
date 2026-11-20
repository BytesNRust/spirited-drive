#include "steps.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the Viewport fits the trace to its size on its third frame: the mouse goes in after SETTLED_FRAMES; the game's log and its end are
 * waited for, not for ever */
enum { DRAG_MOVES = 4, SETTLED_FRAMES = 4, RACE_LOG_FRAMES = 4000, RACE_STOP_FRAMES = 1000 };

static const char *const NAMES[] = { "edit", "undo", "redo", "save", "title", "print", "key", "click", "drag", "width", "race", "race-log", "race-stop", "bank", "drag-near", "problems", "build" };
static const int ARGS[] = { 0, 0, 0, 0, 0, 0, 0, 1, 3, 2, 0, 0, 0, 2, 3, 0, 0 };   /* the numbers each takes (key takes its name after the colon) */

static int known(const char *name)
{
    for (size_t i = 0; i < sizeof NAMES / sizeof NAMES[0]; ++i)
        if (strcmp(name, NAMES[i]) == 0) return (int)i;
    return -1;
}

int steps_read(Steps *s, const char *text)
{
    memset(s, 0, sizeof *s);
    char list[1024];
    snprintf(list, sizeof list, "%s", text);
    for (char *item = strtok(list, ","); item != NULL; item = strtok(NULL, ",")) {
        char *end = NULL;
        const double n = strtod(item, &end);
        if (end != item && *end == '\0' && s->count > 0) {   /* a number: the step before it takes it */
            const int k = s->count - 1;
            if (s->args[k] < STEPS_ARGS) s->arg[k][s->args[k]++] = n;
            continue;
        }
        char word[16] = "";
        snprintf(word, sizeof word, "%.*s", (int)strcspn(item, ":"), item);
        const int which = known(word);
        if (which < 0 || s->count == STEPS_MAX) {
            fprintf(stderr, "--steps %s: edit, undo, redo, save, title, print, key:NAME, click:I, drag:I,DX,DY, drag-near:I,J,M, width:I,M, bank:I,DEG, problems, build, race, race-log or race-stop expected\n", item);
            return 0;
        }
        snprintf(s->name[s->count], sizeof s->name[0], "%s", item);   /* "key:N", "click:2": the first number is after the colon */
        const char *colon = strchr(item, ':');
        if (colon != NULL && which != 6) s->arg[s->count][s->args[s->count]++] = strtod(colon + 1, NULL);
        s->count++;
    }
    for (int k = 0; k < s->count; ++k) {
        char word[16];
        snprintf(word, sizeof word, "%.*s", (int)strcspn(s->name[k], ":"), s->name[k]);
        if (s->args[k] != ARGS[known(word)]) {
            fprintf(stderr, "--steps %s: %d number%s expected\n", s->name[k], ARGS[known(word)], ARGS[known(word)] == 1 ? "" : "s");
            return 0;
        }
    }
    return 1;
}

int steps_running(const Steps *s)
{
    return !s->failed && s->at < s->count;
}

/* where point `i` is in the window: the Viewport's canvas and the view */
static int point_at(const App *a, int i, float *x, float *y)
{
    if (a->kind != APP_KIND_TRACK || i < 0 || i >= a->trace.count) return 0;
    double sx, sy;
    view_to_screen(&a->view, a->trace.points[i].x, a->trace.points[i].z, &sx, &sy);
    *x = a->canvas_x + (float)sx, *y = a->canvas_y + (float)sy;
    return 1;
}

/* where in the window the place `metres` before point `j`, on the line from it to point `i`, is: the target of drag-near */
static int near_point(const App *a, int i, int j, double metres, Steps *s)
{
    if (a->kind != APP_KIND_TRACK || i < 0 || j < 0 || i >= a->trace.count || j >= a->trace.count || i == j) return 0;
    const double dx = a->trace.points[i].x - a->trace.points[j].x, dz = a->trace.points[i].z - a->trace.points[j].z, d = sqrt(dx * dx + dz * dz);
    if (d <= 0) return 0;
    double sx, sy;
    view_to_screen(&a->view, a->trace.points[j].x + dx * metres / d, a->trace.points[j].z + dz * metres / d, &sx, &sy);
    s->to_x = a->canvas_x + (float)sx, s->to_y = a->canvas_y + (float)sy;
    return 1;
}

static void mouse(SDL_Window *win, Uint32 type, float x, float y)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        ev.motion.windowID = SDL_GetWindowID(win);
        ev.motion.x = x, ev.motion.y = y;
    } else {
        ev.button.windowID = SDL_GetWindowID(win);
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        ev.button.clicks = 1;
        ev.button.x = x, ev.button.y = y;
    }
    SDL_PushEvent(&ev);
}

static int key(SDL_Window *win, const char *name, int down)
{
    const SDL_Keycode code = SDL_GetKeyFromName(name);
    if (code == SDLK_UNKNOWN) return 0;
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.windowID = SDL_GetWindowID(win);
    ev.key.key = code;
    ev.key.scancode = SDL_GetScancodeFromKey(code, NULL);
    ev.key.down = down;
    SDL_PushEvent(&ev);
    return 1;
}

static void print_points(const App *a)
{
    fprintf(stdout, "points: %d", a->trace.count);
    for (int i = 0; i < a->trace.count; ++i) fprintf(stdout, " %.3f,%.3f", a->trace.points[i].x, a->trace.points[i].z);
    fprintf(stdout, "\n");
}

/* what Problems shows first: Build's refusal line, else its first red row's node, else none */
static void print_problems(const App *a)
{
    const char *refusal = app_road_refusal(a);
    int red = -1;
    for (int i = 0; red < 0 && i < a->problem_count; ++i)
        if (a->problems[i].red) red = a->problems[i].node;
    if (refusal != NULL) fprintf(stdout, "problems: refusal %s\n", refusal);
    else if (red >= 0) fprintf(stdout, "problems: red node %d\n", red);
    else fprintf(stdout, "problems: none\n");
}

/* a step that needs no frame; 1 when it did what it says */
static int at_once(App *a, const char *name, const double *arg)
{
    if (strcmp(name, "edit") == 0 && a->kind == APP_KIND_TRACK) {
        app_add_point(a);
        app_track_commit(a);   /* as the window does once an edit is done */
        return 1;
    }
    if (strcmp(name, "edit") == 0) return a->kind == APP_KIND_CAR && a->mask.sel >= 0 && mask_edit_pencil(&a->mask.me, 0, 0, 1, a->mask.me.now[0] == 248 ? 0 : 248) > 0;
    if (strcmp(name, "undo") == 0) return app_undo(a);
    if (strcmp(name, "redo") == 0) return app_redo(a);
    if (strcmp(name, "save") == 0) return app_save(a);
    if (strcmp(name, "title") == 0) {
        char title[APP_PATH + 64];
        app_title(a, title, sizeof title);
        fprintf(stdout, "title: %s\n", title);
        return 1;
    }
    if (strcmp(name, "problems") == 0) return a->kind == APP_KIND_TRACK ? (print_problems(a), 1) : 0;
    if (strcmp(name, "build") == 0) {   /* as the Build button: what it says printed, a refusal too */
        app_build(a);
        fprintf(stdout, "build: %s\n", a->status);
        return 1;
    }
    if (strcmp(name, "print") == 0) return a->kind == APP_KIND_TRACK ? (print_points(a), 1) : 0;
    if (strcmp(name, "race") == 0) {   /* Test in race, as its button: the game's command line and the TNFS_ part of its environment printed */
        if (!app_race(a, 0)) return 0;
        fprintf(stdout, "race:");
        for (int i = 0; i < a->race.command.argc; ++i) fprintf(stdout, " %s", a->race.command.argv[i]);
        fprintf(stdout, "\n");
        for (int i = 0; i < a->race.command.envc; ++i)
            if (strncmp(a->race.command.envp[i], "TNFS_", 5) == 0) fprintf(stdout, "race env: %s\n", a->race.command.envp[i]);
        return 1;
    }
    if (strncmp(name, "width", 5) == 0) {
        const int point = (int)arg[0];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
        app_set_point_width(a, point, arg[1]);
        return 1;
    }
    if (strncmp(name, "bank", 4) == 0) {   /* a cell of the Road profile's bank row, as the panel sets it */
        const int point = (int)arg[0];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
        app_set_point_bank(a, point, arg[1]);
        return 1;
    }
    return -1;   /* not one: it takes frames */
}

/* the game's log of the race running has a car of the race: that line printed; 0 while not yet */
static int race_logged(const App *a)
{
    char path[EXPORT_PATH + 32], line[512];
    snprintf(path, sizeof path, "%s/spirited-drive.log", a->race.own[3]);
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    int found = 0;
    while (!found && fgets(line, sizeof line, f) != NULL) found = strncmp(line, "race car: slot 0 is ", 20) == 0;
    fclose(f);
    if (found) fprintf(stdout, "race log: %s", line);
    return found;
}

/* a frame of a step that waits on the game: 1 while it waits, 0 when done (s->failed when it cannot be) */
static int race_frames(Steps *s, App *a, const char *name, int p)
{
    if (strcmp(name, "race-log") == 0) {
        if (race_logged(a)) return 0;
        if (!car_race_busy(&a->race) || p >= RACE_LOG_FRAMES) {
            fprintf(stderr, "--steps race-log: %s\n", car_race_busy(&a->race) ? "no car of the race in the game's log yet" : "the game ended first");
            return s->failed = 1, 0;
        }
        return 1;
    }
    if (p == 0) car_race_stop(&a->race);   /* race-stop: as the Stop button; the window's poll ends the job */
    if (!car_race_busy(&a->race)) {
        fprintf(stdout, "race end: %s\n", a->race.summary);
        return 0;
    }
    if (p >= RACE_STOP_FRAMES) return fprintf(stderr, "--steps race-stop: the game did not end\n"), s->failed = 1, 0;
    return 1;
}

/* the next frame's events of an input step; 1 while it has more, 0 when it is done (its last event taken and a frame drawn since) */
static int by_frames(Steps *s, App *a, SDL_Window *win, const char *name, const double *arg)
{
    const int p = s->phase++;
    float x = 0, y = 0;
    if (strncmp(name, "race-", 5) == 0) return race_frames(s, a, name, p);
    if (strncmp(name, "key:", 4) == 0) {
        if (p == 0 && !key(win, name + 4, 1)) return s->failed = 1, 0;
        if (p == 1) key(win, name + 4, 0);
        return p < 2;
    }
    if (strncmp(name, "click:", 6) == 0) {
        if (p == 0 && !point_at(a, (int)arg[0], &s->from_x, &s->from_y)) return s->failed = 1, 0;
        x = s->from_x, y = s->from_y;
        if (p == 0) mouse(win, SDL_EVENT_MOUSE_MOTION, x, y);
        if (p == 1) mouse(win, SDL_EVENT_MOUSE_BUTTON_DOWN, x, y);
        if (p == 2) mouse(win, SDL_EVENT_MOUSE_BUTTON_UP, x, y);
        return p < 3;
    }
    /* drag:I,DX,DY: to the point, down, DRAG_MOVES moves, up; drag-near:I,J,M the same to M metres before point J on the way from I */
    if (p == 0) {
        if (!point_at(a, (int)arg[0], &s->from_x, &s->from_y)) return s->failed = 1, 0;
        if (strncmp(name, "drag-near", 9) == 0 && !near_point(a, (int)arg[0], (int)arg[1], arg[2], s)) return s->failed = 1, 0;
        if (strncmp(name, "drag-near", 9) != 0) s->to_x = s->from_x + (float)arg[1], s->to_y = s->from_y + (float)arg[2];
        mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y);
    } else if (p == 1) {
        mouse(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y);
    } else if (p <= 1 + DRAG_MOVES) {
        const float f = (float)(p - 1) / DRAG_MOVES;
        mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x + f * (s->to_x - s->from_x), s->from_y + f * (s->to_y - s->from_y));
    } else if (p == 2 + DRAG_MOVES) {
        mouse(win, SDL_EVENT_MOUSE_BUTTON_UP, s->to_x, s->to_y);
    }
    return p < 3 + DRAG_MOVES;
}

void steps_frame(Steps *s, App *a, SDL_Window *win)
{
    s->frames++;
    while (steps_running(s)) {
        const char *name = s->name[s->at];
        const double *arg = s->arg[s->at];
        const int done = at_once(a, name, arg);
        if (done == 0) {
            fprintf(stderr, "--steps: %s did nothing (%s)\n", name, a->status);
            s->failed = 1;
            return;
        }
        if (done == 1) {
            s->at++;
            continue;
        }
        if (s->frames < SETTLED_FRAMES) return;
        if (by_frames(s, a, win, name, arg)) return;   /* its events go in with the next frame */
        if (s->failed) {
            if (strncmp(name, "race-", 5) != 0) fprintf(stderr, "--steps: %s cannot be done (no such key or point)\n", name);
            return;
        }
        s->at++, s->phase = 0;
        return;   /* the frame after the step's last event drew what it did: the next step starts with the next frame */
    }
}
