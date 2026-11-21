#include "steps.h"

#include "app_points.h"
#include "app_select.h"
#include "app_colour.h"
#include "app_sheet.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the Viewport fits the trace to its size on its third frame: the mouse goes in after SETTLED_FRAMES; the game's log and its end are
 * waited for, not for ever */
enum { DRAG_MOVES = 4, SETTLED_FRAMES = 4, RACE_LOG_FRAMES = 4000, RACE_STOP_FRAMES = 1000, SHEET_FRAMES = 12000 };

static const char *const NAMES[] = { "edit", "undo", "redo", "save", "title", "print", "key", "click", "drag", "width", "race", "race-log", "race-stop", "bank", "drag-near", "problems", "build",
                                     "shift-click", "cmd-click", "box", "row", "shift-row", "cmd-row", "rotate", "print-sel", "print-profile", "status", "copy", "paste", "mirror-lr", "mirror-fb", "reverse", "curve", "straight", "preview", "curve-block", "curve-fold", "colour-by", "dblclick-road", "print-classes", "sheet", "print-sheet", "sheet-save", "height" };
static const int ARGS[] = { 0, 0, 0, 0, 0, 0, 0, 1, 3, 2, 0, 0, 0, 2, 3, 0, 0, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 1, 0, 1, 1, 2, 0, 1, 0, 0, 2 };   /* the numbers each takes (key takes its name after the colon) */

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
            fprintf(stderr, "--steps %s: edit, undo, redo, save, title, print, key:NAME, click:I, drag:I,DX,DY, drag-near:I,J,M, width:I,M, bank:I,DEG, height:I,M, problems, build, shift-click:I, cmd-click:I, box:X0,Z0,X1,Z1, row:I, shift-row:I, cmd-row:I, rotate:DEG, print-sel, print-profile, race, race-log or race-stop expected\n", item);
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

static void mouse_clicks(SDL_Window *win, Uint32 type, float x, float y, int clicks)
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
        ev.button.clicks = (Uint8)clicks;
        ev.button.x = x, ev.button.y = y;
    }
    SDL_PushEvent(&ev);
}

static void mouse(SDL_Window *win, Uint32 type, float x, float y)
{
    mouse_clicks(win, type, x, y, 1);
}

/* a key's event; `mod` the modifiers held with it, as the keyboard sends them (the window takes them from the event, not the state) */
static int key_event(SDL_Window *win, SDL_Keycode code, int down, SDL_Keymod mod)
{
    if (code == SDLK_UNKNOWN) return 0;
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.windowID = SDL_GetWindowID(win);
    ev.key.key = code;
    ev.key.scancode = SDL_GetScancodeFromKey(code, NULL);
    ev.key.down = down;
    ev.key.mod = mod;
    SDL_PushEvent(&ev);
    return 1;
}

/* the modifier a step holds: "Shift+" or "Cmd+" before a key's name, or a shift- / cmd- step; *rest the key's name after it */
static void held(const char *name, SDL_Keycode *code, SDL_Keymod *mod, const char **rest)
{
    *code = SDLK_UNKNOWN, *mod = SDL_KMOD_NONE, *rest = name;
    if (strncmp(name, "Shift+", 6) == 0 || strncmp(name, "shift-", 6) == 0) *code = SDLK_LSHIFT, *mod = SDL_KMOD_LSHIFT, *rest = name + 6;
    else if (strncmp(name, "Cmd+", 4) == 0 || strncmp(name, "cmd-", 4) == 0) *code = SDLK_LGUI, *mod = SDL_KMOD_LGUI, *rest = name + 4;
}

/* key:NAME with its modifier: the modifier down, the key down and up with it held, the modifier up; a frame each */
static int key_frames(Steps *s, SDL_Window *win, const char *name, int p)
{
    SDL_Keycode mod_key;
    SDL_Keymod mod;
    const char *rest;
    held(name, &mod_key, &mod, &rest);
    const SDL_Keycode code = SDL_GetKeyFromName(rest);
    if (p == 0 && code == SDLK_UNKNOWN) return s->failed = 1, 0;
    if (p == 0 && mod_key != SDLK_UNKNOWN) key_event(win, mod_key, 1, mod);
    if (p == 1) key_event(win, code, 1, mod);
    if (p == 2) key_event(win, code, 0, mod);
    if (p == 3 && mod_key != SDLK_UNKNOWN) key_event(win, mod_key, 0, SDL_KMOD_NONE);
    return p < 4;
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
    if (strcmp(name, "problems") == 0) return a->kind == APP_KIND_TRACK ? (app_recompute(a), print_problems(a), 1) : 0;   /* an edit this frame counts */
    if (strcmp(name, "build") == 0) {   /* as the Build button: what it says printed, a refusal too */
        app_build(a);
        fprintf(stdout, "build: %s\n", a->status);
        return 1;
    }
    if (strcmp(name, "print") == 0) return a->kind == APP_KIND_TRACK ? (print_points(a), 1) : 0;
    if (strcmp(name, "status") == 0) return fprintf(stdout, "status: %s\n", a->status), 1;
    if (strcmp(name, "copy") == 0) return app_copy_points(a);   /* each is one step to undo (app_points.h) */
    if (strcmp(name, "paste") == 0) return app_paste_points(a);
    if (strcmp(name, "mirror-lr") == 0) return app_mirror_points(a, 0);
    if (strcmp(name, "mirror-fb") == 0) return app_mirror_points(a, 1);
    if (strcmp(name, "reverse") == 0) return app_reverse_points(a);
    if (strncmp(name, "curve:", 6) == 0 || strncmp(name, "straight:", 9) == 0) {   /* the Add curve / Add straight fields set, then Accept */
        app_curve_defaults(a);
        if (name[0] == 'c') a->curve.right = arg[0] != 0, a->curve.degrees = arg[1], a->curve.radius_m = arg[2], a->curve.count = (int)arg[3];
        else a->curve.length_m = arg[0], a->curve.straight_count = (int)arg[1];
        return app_add_curve(a, name[0] == 's');
    }
    if (strncmp(name, "sheet:", 6) == 0) return arg[0] == 0 || arg[0] == 1 ? 2 : 0;   /* by_frames: the sheet is taken, one photo after another */
    if (strcmp(name, "print-sheet") == 0) {
        char text[1024];
        app_sheet_describe(a, text, sizeof text);
        return fprintf(stdout, "%s\n", text), 1;
    }
    if (strcmp(name, "sheet-save") == 0) return app_sheet_save(a) ? (fprintf(stdout, "sheet saved: %s\n", a->sheet_saved), 1) : 0;
    if (strncmp(name, "preview:", 8) == 0) return arg[0] >= 0 && arg[0] <= 2 ? (app_curve_defaults(a), a->curve.preview_hold = (int)arg[0], 1) : 0;
    if (strcmp(name, "curve-block") == 0 || strncmp(name, "curve-fold:", 11) == 0) return arg[0] == 0 || arg[0] == 1 || name[6] == 'b' ? 2 : 0;   /* by_frames: after the panel has drawn */
    if (strncmp(name, "colour-by:", 10) == 0) return arg[0] >= 0 && arg[0] < TRACE_COLOUR_MODES ? (app_set_colour_by(a, (int)arg[0]), 1) : 0;   /* a mode of TraceColourBy */
    if (strcmp(name, "print-classes") == 0) {
        char text[1024];
        app_colour_summary(a, text, sizeof text);
        return fprintf(stdout, "%s\n", text), 1;
    }
    if (strncmp(name, "dblclick-road:", 14) == 0) return 2;   /* by_frames: the mouse goes in */
    if (strcmp(name, "print-sel") == 0) {
        if (a->kind != APP_KIND_TRACK) return 0;
        char which[256];
        app_sel_sync(a);
        sel_describe(&a->sel, which, sizeof which);
        fprintf(stdout, "selected: %s\n", which[0] != '\0' ? which : "none");
        return 1;
    }
    if (strcmp(name, "print-profile") == 0) {
        if (a->kind != APP_KIND_TRACK) return 0;
        app_sel_sync(a);
        fprintf(stdout, "profile:");
        for (int row = 0; row < APP_SEL_PROFILE_ROWS; ++row) {
            char left[32], right[32];
            if (app_sel_profile(a, row, TRACK_GEN_LEFT, left, sizeof left) < 0 || app_sel_profile(a, row, TRACK_GEN_RIGHT, right, sizeof right) < 0) return 0;
            fprintf(stdout, " %s|%s", left, right);
        }
        fprintf(stdout, "\n");
        return 1;
    }
    if (strncmp(name, "row:", 4) == 0 || strncmp(name, "shift-row:", 10) == 0 || strncmp(name, "cmd-row:", 8) == 0) {   /* as the Points table's row */
        const int point = (int)arg[0];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
        app_sel_click(a, point, name[0] == 's' ? SEL_RANGE : name[0] == 'c' ? SEL_TOGGLE : SEL_ONE);
        return 1;
    }
    if (strncmp(name, "rotate", 6) == 0) {   /* Rotate points, typed: its own undo step, as the field's let go is */
        if (a->kind != APP_KIND_TRACK || app_sel_count(a) == 0) return 0;
        app_sel_rotate_by(a, arg[0]);
        app_track_commit(a);
        return 1;
    }
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
    if (strncmp(name, "height", 6) == 0) {   /* a point's height, as the Points window's h cell and the profile in the Distance strip set it */
        const int point = (int)arg[0];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
        app_set_point_height(a, point, arg[1]);
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
    if (strcmp(name, "curve-block") == 0) {   /* the block as the panel drew it in the frame before */
        fprintf(stdout, "curve block: %s\n", a->curve.block_open == 1 ? "open" : a->curve.block_open == 2 ? "folded" : "not drawn");
        return 0;
    }
    if (strncmp(name, "curve-fold:", 11) == 0) {   /* a click on the header: set now, drawn by the next frame, which this step waits for */
        if (p == 0) a->curve.block_force = arg[0] != 0 ? 1 : 2;
        return p == 0;
    }
    if (strncmp(name, "dblclick-road:", 14) == 0) {   /* dblclick-road:X,Z: a double click on the road where track place (X, Z) is, in metres */
        if (p == 0) {
            double sx, sy;
            if (a->kind != APP_KIND_TRACK) return s->failed = 1, 0;
            view_to_screen(&a->view, arg[0], arg[1], &sx, &sy);
            s->from_x = a->canvas_x + (float)sx, s->from_y = a->canvas_y + (float)sy;
        }
        if (p == 0) mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y);
        if (p == 1) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y, 1);
        if (p == 2) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_UP, s->from_x, s->from_y, 1);
        if (p == 3) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y, 2);
        if (p == 4) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_UP, s->from_x, s->from_y, 2);
        return p < 5;
    }
    if (strncmp(name, "sheet:", 6) == 0) {   /* sheet:N, N 0 the car's, 1 the traffic's: started, then waited for (a sheet is a few photos of a game run) */
        if (p == 0 && !app_sheet_start(a, (int)arg[0])) return s->failed = 1, 0;
        if (p >= SHEET_FRAMES) return fprintf(stderr, "--steps sheet: it did not end\n"), s->failed = 1, 0;
        return app_sheet_busy(a);
    }
    if (strncmp(name, "key:", 4) == 0) return key_frames(s, win, name + 4, p);
    if (strncmp(name, "click:", 6) == 0 || strstr(name, "-click:") != NULL) {   /* shift-click, cmd-click: the modifier held around it */
        SDL_Keycode mod_key;
        SDL_Keymod mod;
        const char *rest;
        held(name, &mod_key, &mod, &rest);
        if (p == 0 && !point_at(a, (int)arg[0], &s->from_x, &s->from_y)) return s->failed = 1, 0;
        x = s->from_x, y = s->from_y;
        if (p == 0 && mod_key != SDLK_UNKNOWN) key_event(win, mod_key, 1, mod);
        if (p == 1) mouse(win, SDL_EVENT_MOUSE_MOTION, x, y);
        if (p == 2) mouse(win, SDL_EVENT_MOUSE_BUTTON_DOWN, x, y);
        if (p == 3) mouse(win, SDL_EVENT_MOUSE_BUTTON_UP, x, y);
        if (p == 4 && mod_key != SDLK_UNKNOWN) key_event(win, mod_key, 0, SDL_KMOD_NONE);
        return p < 5;
    }
    if (strncmp(name, "box", 3) == 0 && p == 0) {   /* box:X0,Z0,X1,Z1: the corners from track places, then the drag's frames */
        if (a->kind != APP_KIND_TRACK) return s->failed = 1, 0;
        double sx, sy;
        view_to_screen(&a->view, arg[0], arg[1], &sx, &sy);
        s->from_x = a->canvas_x + (float)sx, s->from_y = a->canvas_y + (float)sy;
        view_to_screen(&a->view, arg[2], arg[3], &sx, &sy);
        s->to_x = a->canvas_x + (float)sx, s->to_y = a->canvas_y + (float)sy;
        mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y);
        return 1;
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
