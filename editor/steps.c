#include "steps.h"
#include "ui_probe.h"
#include "formats/car_piece.h"
#include "car_physics_from.h"
#include "app_slope.h"
#include "key_ops.h"

#include "ui.h"

#include "app_points.h"
#include "app_select.h"
#include "app_colour.h"
#include "app_sheet.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* the Viewport fits the trace to its size on its third frame: the mouse goes in after SETTLED_FRAMES; the game's log and its end are
 * waited for, not for ever */
enum { DRAG_MOVES = 4, SETTLED_FRAMES = 4, RACE_LOG_FRAMES = 4000, RACE_STOP_FRAMES = 1000, SHEET_FRAMES = 12000 };

int shine_draw_get_soft(void);   /* core/raster/shine_draw.h, not included: its View clashes with the editor's */
int shine_draw_get_render_scale(void);

static const char *const NAMES[] = { "edit", "undo", "redo", "save", "title", "print", "key", "click", "drag", "width", "race", "race-log", "race-stop", "bank", "drag-near", "problems", "build",
                                     "shift-click", "cmd-click", "box", "row", "shift-row", "cmd-row", "rotate", "print-sel", "print-profile", "status", "copy", "paste", "mirror-lr", "mirror-fb", "reverse", "curve", "straight", "preview", "curve-block", "curve-fold", "colour-by", "dblclick-road", "print-classes", "sheet", "print-sheet", "sheet-save", "height", "strip", "profile-drag", "print-height", "strip-dblclick", "print-view", "profile-fine", "profile-click", "insert-here", "slope", "slope-rise", "slope-preview", "shift-dblclick", "cmd-dblclick", "lanes", "extend", "extend-end", "print-road", "copy-profile", "paste-profile", "side", "print-mixed", "print-workspace", "open-ws", "dialog-dir", "no-workspace", "panel", "print-panels", "print-menu", "main-menu", "new-track", "new-track-open", "strip-split", "print-strip", "strip-size", "night-light", "night-tint", "night-reset", "print-night", "lights-open", "card-photo", "card-view", "card-backdrop", "card-bd-bottom", "ui-fault", "scan", "add-originals", "open-original", "publish-ask", "publish", "print-original", "original-open", "masks-print", "mask-select", "mask-regen", "check-originals", "masks-poll", "repo-ask", "repo-copy", "print-repo", "print-confirm", "poll-wait", "check", "new-car-open", "new-car", "physics-from", "physics-open", "physics-print", "export-open", "export-track", "masks-only", "print-publish", "light-drive", "drive-stop", "light-weather", "print-drive", "light-bright", "light-tone", "light-lamp", "light-remove", "ui-click", "ui-at", "ui-span", "ui-text", "ui-focus", "print-tunnels", "tunnel-lamps", "sun", "light-print", "light-node", "light-copy", "light-copy-answer", "light-note", "repo-choose", "camera", "car-buttons", "paint-rgb", "paint-print", "psd-wait", "reset-layout", "ui-rect", "light-state", "tweak-make", "tweak-apply" };
static const int ARGS[] = { 0, 0, 0, 0, 0, 0, 0, 1, 3, 2, 0, 0, 0, 2, 3, 0, 0, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 1, 0, 1, 1, 2, 0, 0, 0, 0, 2, 0, 2, 1, 1, 0, 2, 2, 0, 4, 4, 1, 2, 2, 2, 2, 2, 0, 0, 0, 4, 0, 0, 2, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 3, 1, 0, 0, 0, 2, 4, 3, 0, 2, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 2, 0, 0, 0, 2, 3, 0, 0, 4, 1, 0, 0, 1, 0, 4, 0, 1, 0, 0, 0, 0, 0 };   /* the numbers each takes (key takes its name after the colon) */

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
            fprintf(stderr, "--steps %s: edit, undo, redo, save, title, print, key:NAME, click:I, drag:I,DX,DY, drag-near:I,J,M, width:I,M, bank:I,DEG, height:I,M, strip, profile-drag:I,DY, print-height:I, problems, build, shift-click:I, cmd-click:I, box:X0,Z0,X1,Z1, row:I, shift-row:I, cmd-row:I, rotate:DEG, print-sel, print-profile, race, race-log or race-stop expected\n", item);
            return 0;
        }
        snprintf(s->name[s->count], sizeof s->name[0], "%s", item);   /* "key:N", "click:2": the first number is after the colon */
        const char *colon = strchr(item, ':');
        if (colon != NULL && which != 6 && strncmp(word, "ui-", 3) != 0 && strncmp(word, "tweak-", 6) != 0) s->arg[s->count][s->args[s->count]++] = strtod(colon + 1, NULL);
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

/* where in the window point i's mark in the Distance strip's profile is, as the strip drew it last frame; 0 when it is not on screen */
static int mark_at(const App *a, int i, float *x, float *y)
{
    const double span = a->profile_drawn.hi - a->profile_drawn.lo;
    if (a->kind != APP_KIND_TRACK || !a->profile_drawn.drawn || i < 0 || i >= a->trace.count || a->trace.count < 2 || !(span > 0)) return 0;
    double *metres = malloc(sizeof *metres * (size_t)a->trace.count);
    if (metres == NULL) return 0;
    track_trace_point_distances(a->trace.points, a->trace.count, metres);
    *x = a->profile_drawn.x + (float)(metres[i] / a->profile_drawn.total) * a->profile_drawn.width;
    *y = a->profile_drawn.top + (float)((a->profile_drawn.hi - a->trace.points[i].h) / span) * (a->profile_drawn.bottom - a->profile_drawn.top);
    free(metres);
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
    if (strcmp(name, "undo") == 0) return app_undo(a);
    if (strcmp(name, "redo") == 0) return app_redo(a);
    if (strcmp(name, "save") == 0) return app_save(a);
    if (strcmp(name, "ui-fault") == 0) return ui_fault(), 1;   /* a deliberate ImGui error: the guard's own test (a headless run must then fail) */
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
    if (strncmp(name, "paint-rgb:", 10) == 0) {   /* paint-rgb:I,R,G,B: the card's I-th Color set, as the colour box does */
        const uint8_t rgb[3] = { (uint8_t)arg[1], (uint8_t)arg[2], (uint8_t)arg[3] };
        return (int)arg[0] >= 0 && (int)arg[0] < a->edit.now.paint_count ? (car_edit_paint_rgb(&a->edit, (int)arg[0], rgb), 1) : 0;
    }
    if (strcmp(name, "paint-print") == 0) {   /* the card's Colors, one line */
        fprintf(stdout, "paints:");
        for (int i = 0; i < a->edit.now.paint_count; i++) fprintf(stdout, " %s #%02x%02x%02x", a->edit.now.paints[i].name, a->edit.now.paints[i].rgb[0], a->edit.now.paints[i].rgb[1], a->edit.now.paints[i].rgb[2]);
        return fprintf(stdout, "\n"), 1;
    }
    if (strcmp(name, "car-buttons") == 0) {   /* what the Cars room's Build and Install are for the open car: the panel's own test */
        return fprintf(stdout, "car buttons: %s\n", !app_car_is_kit(a) ? "no kit" : app_car_is_original(a) ? "off (original)" : "on"), 1;
    }
    if (strcmp(name, "check") == 0) {   /* as the Check button, waited for: one line a finding, then the totals */
        if (!app_check(a)) return fprintf(stdout, "check: %s\n", a->status), 0;
        while (app_car_build_poll(a)) usleep(5000);
        for (size_t i = 0; i < a->car_build.list.count; i++)
            fprintf(stdout, "check %s: %s\n", a->car_build.list.item[i].severity == CAR_CHECK_ERROR ? "error" : "warning", a->car_build.list.item[i].text);
        fprintf(stdout, "check: %s\n", a->status);
        return a->car_build.ok;
    }
    if (strcmp(name, "print") == 0) return a->kind == APP_KIND_TRACK ? (print_points(a), 1) : 0;
    if (strncmp(name, "card-bd-bottom", 14) == 0) return ui_card_backdrop(a, 0, (int)arg[0], (int)arg[1], (int)arg[2], 1);   /* card-bd-bottom:R,G,B */
    if (strncmp(name, "card-backdrop", 13) == 0) return ui_card_backdrop(a, (int)arg[0], (int)arg[1], (int)arg[2], (int)arg[3], 0);   /* card-backdrop:KIND,R,G,B: 0 plain, 1 gradient, 2 picture */
    if (strncmp(name, "card-view", 9) == 0) return ui_card_view(a, (int)arg[0], (int)arg[1]);   /* card-view:CARD,AS_ON_THE_CARD: the Model window's two boxes */
    if (strcmp(name, "card-photo") == 0) return ui_card_photo(a);   /* the Model window's button (its panel has drawn by the time steps run) */
    if (strcmp(name, "status") == 0) return fprintf(stdout, "status: %s\n", a->status), 1;
    if (strcmp(name, "copy") == 0) return app_copy_points(a);   /* each is one step to undo (app_points.h) */
    if (strcmp(name, "paste") == 0) return app_paste_points(a);
    if (strcmp(name, "copy-profile") == 0) return app_copy_profile(a);   /* the Road profile buttons */
    if (strcmp(name, "paste-profile") == 0) return app_paste_profile(a);
    if (strcmp(name, "mirror-lr") == 0) return app_mirror_points(a, 0);
    if (strcmp(name, "mirror-fb") == 0) return app_mirror_points(a, 1);
    if (strcmp(name, "reverse") == 0) return app_reverse_points(a);
    if (strncmp(name, "curve:", 6) == 0 || strncmp(name, "straight:", 9) == 0) {   /* the Add curve / Add straight fields set, then Accept */
        app_curve_defaults(a);
        if (name[0] == 'c') a->curve.right = arg[0] != 0, a->curve.degrees = arg[1], a->curve.radius_m = arg[2], a->curve.count = (int)arg[3];
        else a->curve.length_m = arg[0], a->curve.straight_count = (int)arg[1];
        return app_add_curve(a, name[0] == 's');
    }
    if (strcmp(name, "sheet") == 0) return arg[0] == 0 || arg[0] == 1 ? 2 : 0;   /* by_frames: the sheet is taken, one photo after another */
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
    if (strncmp(name, "dblclick-road:", 14) == 0 || strncmp(name, "strip-dblclick:", 15) == 0 || strncmp(name, "shift-dblclick:", 15) == 0 || strncmp(name, "cmd-dblclick:", 13) == 0) return 2;   /* by_frames: the mouse goes in */
    if (strcmp(name, "new-track-open") == 0) {   /* File > New track with a workspace, the window up (for a picture): a headless run has no native dialog to follow */
        if (a->workspace[0] == '\0') return 0;
        a->new_track_dialog = 1, a->new_track_name[0] = a->new_track_id[0] = a->new_track_error[0] = '\0', a->new_track_id_typed = 0;
        return 1;
    }
    if (strncmp(name, "new-track:", 10) == 0) {   /* new-track:K: File > New track with a workspace, the name "Road K" typed and Create pressed */
        char text[32];
        snprintf(text, sizeof text, "Road %d", (int)arg[0]);
        return app_new_track_named(a, text, NULL);
    }
    if (strcmp(name, "new-car-open") == 0) {   /* File > New car, the window up (for a picture) */
        if (a->workspace[0] == '\0') return 0;
        a->new_car_dialog = 1, a->new_car_name[0] = a->new_car_id[0] = a->new_car_error[0] = '\0', a->new_car_id_typed = 0, a->new_car_template = 0;
        return 1;
    }
    if (strncmp(name, "new-car:", 8) == 0) {   /* new-car:T: File > New car with a workspace, the name "Car <word>" typed, car T (0..7, the eight you drive) chosen and Create pressed */
        const char *word = car_piece_word_of_model((int)arg[0]);
        char text[48];
        if (word == NULL || (int)arg[0] > 7) return 0;
        snprintf(text, sizeof text, "Car %s", word);
        return app_new_car_named(a, text, NULL, word);
    }
    if (strncmp(name, "physics-from:", 13) == 0) {   /* physics-from:T: the Physics from row's car T chosen (0..8: the eight, then the Warrior) and Copy pressed in the question that asks first */
        const char *words[CAR_PHYSICS_WORDS];
        const int n = car_physics_words(words, CAR_PHYSICS_WORDS);
        return (int)arg[0] >= 0 && (int)arg[0] < n && app_physics_from(a, words[(int)arg[0]]);
    }
    if (strcmp(name, "export-open") == 0) {   /* File > Export original track, the window up (for a picture) */
        if (a->workspace[0] == '\0') return 0;
        a->export_track_dialog = 1, a->export_track_name[0] = a->export_track_id[0] = a->export_track_error[0] = '\0', a->export_track_id_typed = 0, a->export_track_pick = 0;
        return 1;
    }
    if (strncmp(name, "export-track:", 13) == 0) {   /* export-track:N: Export original track with a workspace, the name "Road <stem>" typed, the Nth original track (0..15) chosen and Create pressed */
        const char *stem = original_track_stem((int)arg[0]);
        char text[48];
        if (stem == NULL) return 0;
        snprintf(text, sizeof text, "Road %s", stem);
        return app_export_original_track(a, stem, text, NULL);
    }
    if (strcmp(name, "physics-print") == 0) {   /* what the Physics from row shows: «not set» until a car is picked */
        const char *words[CAR_PHYSICS_WORDS];
        const int n = car_physics_words(words, CAR_PHYSICS_WORDS);
        return fprintf(stdout, "physics row: %s\n", a->physics_chosen && a->physics_pick >= 0 && a->physics_pick < n ? words[a->physics_pick] : "not set"), 1;
    }
    if (strncmp(name, "camera:", 7) == 0) {   /* camera:K, the Model's camera: 0 prints it, 1 moves it as a hand would, 2 reads the car again as Convert PSD ends (two frames drawn after) */
        int yaw, pitch, dist;
        if (arg[0] == 1) return ui_camera_nudge(), 1;
        if (arg[0] == 2) return 2;   /* by_frames */
        if (arg[0] == 3) {   /* camera:3 the Model window's state: floating enlarged or not, docked or not, its render scale, its Soft edges switch */
            int big, docked, scale, soft;
            ui_model_state(&big, &docked, &scale, &soft);
            return fprintf(stdout, "model: big %d docked %d scale %d soft %d studio scale %d soft %d\n", big, docked, scale, soft, shine_draw_get_render_scale(), shine_draw_get_soft()), 1;   /* the last two are the process's own, which the Model must leave as the Preview has them */
        }
        if (arg[0] == 4) return fprintf(stdout, "model rebuilds: %d\n", ui_model_rebuilds()), 1;   /* camera:4 the scenes made again for a colour changed */
        ui_camera(&yaw, &pitch, &dist);
        return fprintf(stdout, "camera: %d %d %d\n", yaw, pitch, dist), 1;
    }
    if (strncmp(name, "physics-open:", 13) == 0) {   /* physics-open:T: the question up with car T named (for a picture) */
        a->physics_pick = (int)arg[0], a->physics_chosen = 1, a->physics_dialog = 1;
        return 1;
    }
    if (strcmp(name, "tweak-make") == 0) return app_tweak_make(a);   /* the open original's tweak folder in <workspace>/tweaks/ */
    if (strncmp(name, "tweak-apply:", 12) == 0) return app_tweak_apply(a, name + 12);   /* tweak-apply:ID, the tweak folder <workspace>/tweaks/ID, into the game repo */
    if (strcmp(name, "add-originals") == 0) return app_original_add_all(a);   /* the Add the 16 original tracks button */
    if (strncmp(name, "open-original:", 14) == 0) {   /* open-original:I: the I-th of the 16 CD tracks (AL1 is 0) opened from the workspace, as a click in Open original track does (a headless run drops the question about unsaved changes) */
        char dir[APP_PATH];
        return original_track_dir(a->workspace, original_track_stem((int)arg[0]), dir, sizeof dir) && app_open_original(a, dir);
    }
    if (strcmp(name, "original-open") == 0) return a->original_dialog = 1, 1;   /* File > Open original track..., the window up (for a picture) */
    if (strcmp(name, "masks-print") == 0) {   /* the original track's masks (scanned and made from the PSDs as the panel does): the totals, then a line a mask */
        if (!app_track_masks_load(a)) return fprintf(stdout, "masks: none (%s)\n", a->tmask_note), 0;
        fprintf(stdout, "masks: %d to_make %d edited %d\n", a->tmasks.count, a->tmasks.to_make, a->tmasks.edited);
        for (int i = 0; i < a->tmasks.count; ++i) {
            const TrackMaskRow *r = &a->tmasks.row[i];
            fprintf(stdout, "mask %s: %s, %dx%d, %d reflective, %d lamps%s%s\n", r->name, track_mask_state_text(r->state), r->width, r->height, r->reflective, r->lamps, r->note[0] != '\0' ? ", " : "", r->note);
        }
        return 1;
    }
    if (strcmp(name, "check-originals") == 0) {   /* the migration check: every original in the workspace has its PNGs made from its PSDs and compared with the game's data/ */
        char text[1024];
        const int ok = app_original_check_all(a, text, sizeof text);
        return fprintf(stdout, "%s\n", text), ok;
    }
    if (strcmp(name, "repo-choose") == 0) {   /* File > Game repo folder...: the folder the dialog would give is TNFS_STUDIO_PICK; a refusal is in the status, not a failed step */
        const char *dir = getenv("TNFS_STUDIO_PICK");
        return dir != NULL && (app_repo_data_choose(a, dir), 1);
    }
    if (strcmp(name, "print-tunnels") == 0 || strncmp(name, "tunnel-lamps", 12) == 0) {   /* the Tunnels section of the Track light window: the track's tunnel facts with the time's look of each */
        if (a->kind != APP_KIND_LIGHT || !a->light.session.has_file) return fprintf(stderr, "tunnels: no light file open\n"), 0;
        LightTrack *f = &a->light.session.file;
        const int time = a->light.time, n = light_track_count(f, LT_TIME_FACTS, LT_TUNNEL), looks = light_track_count(f, time, LT_LOOK);
        if (strcmp(name, "print-tunnels") == 0) {
            fprintf(stdout, "tunnels: %d\n", n);
            for (int j = 0; j < looks; ++j) {
                LightTrackLook v;
                if (light_track_get_look(f, time, j, &v) && !v.has_nodes) fprintf(stdout, "default: light %s%g lamps %s%g reach %s%g\n", v.has_light ? "" : "- ", v.light, v.has_lamps ? "" : "- ", v.lamps, v.has_reach ? "" : "- ", v.reach);
            }
            for (int i = 0; i < n; ++i) {
                LightTrackTunnel t;
                if (!light_track_get_tunnel(f, i, &t)) continue;
                LightTrackLook own;
                int have = 0;
                for (int j = 0; j < looks && !have; ++j) have = light_track_get_look(f, time, j, &own) && own.has_nodes && own.from == t.from && own.to == t.to;
                fprintf(stdout, "tunnel %d: %u-%u%s", i, t.from, t.to, t.lit ? " hall" : "");
                if (have) fprintf(stdout, " own: light %s%g lamps %s%g reach %s%g", own.has_light ? "" : "- ", own.light, own.has_lamps ? "" : "- ", own.lamps, own.has_reach ? "" : "- ", own.reach);
                fprintf(stdout, "\n");
            }
            return 1;
        }
        const int at = (int)arg[0];   /* tunnel-lamps:I,S: the lamps' strength of tunnel I's look at the time shown, S percent */
        LightTrackTunnel t;
        char why[LT_WHY] = "";
        if (at < 0 || at >= n || !light_track_get_tunnel(f, at, &t)) return fprintf(stderr, "tunnels: no tunnel %d\n", at), 0;
        LightTrackLook v;
        int index = -1;
        for (int j = 0; j < looks && index < 0; ++j)
            if (light_track_get_look(f, time, j, &v) && v.has_nodes && v.from == t.from && v.to == t.to) index = j;
        if (index < 0) memset(&v, 0, sizeof v), v.has_nodes = 1, v.from = t.from, v.to = t.to;
        v.has_lamps = 1, v.lamps = (double)arg[1];
        if (!light_track_set_look(f, time, index, &v, why, sizeof why)) return fprintf(stderr, "tunnels: %s\n", why), 0;
        return app_light_touch(a), 1;
    }
    if (strncmp(name, "sun:", 4) == 0) {   /* sun:AZ,EL,SH: the time's sun (direction and height in degrees, the shadow's strength in percent) */
        if (a->kind != APP_KIND_LIGHT || !a->light.session.has_file) return fprintf(stderr, "sun: no light file open\n"), 0;
        const LightTrackSun s = { 1, 1, 1, (double)arg[0], (double)arg[1], (double)arg[2], 0, 0.0 };
        char why[LT_WHY];
        if (!light_track_set_sun(&a->light.session.file, a->light.time, &s, why, sizeof why)) return fprintf(stderr, "sun: %s\n", why), 0;
        return app_light_touch(a), 1;
    }
    if (strcmp(name, "light-print") == 0) {   /* the open light file as the panel sees it: path, nodes, the time shown and what its block holds */
        if (a->kind != APP_KIND_LIGHT) return fprintf(stderr, "light-print: no light window\n"), 0;
        const LightTrackSession *s = &a->light.session;
        if (!s->has_file) return fprintf(stdout, "light: %s no file\n", s->track), 1;
        const LightTrack *f = &s->file;
        const int time = a->light.time;
        LightTrackSun sun;
        const int has_sun = light_track_get_sun(f, time, &sun);
        fprintf(stdout, "light: %s nodes %u time %c%s tunnels %d looks %d lamps %d brightness %d tones %d", s->track, s->nodes, light_ui_time_letter(time), light_track_session_dirty(s) ? " (unsaved)" : "",
                light_track_count(f, LT_TIME_FACTS, LT_TUNNEL), light_track_count(f, time, LT_LOOK), light_track_count(f, time, LT_LAMP), light_track_count(f, time, LT_BRIGHTNESS), light_track_count(f, time, LT_TONE));
        if (has_sun) fprintf(stdout, " sun %s%g %s%g %s%g", sun.has_azimuth ? "" : "-", sun.azimuth, sun.has_elevation ? "" : "-", sun.elevation, sun.has_shadow ? "" : "-", sun.shadow);
        return fprintf(stdout, "\n"), 1;
    }
    if (strcmp(name, "light-state") == 0) {   /* the Track light window's track, view, HUD, compare mode, test drive state and the last refusal, as the panel has them */
        if (a->kind != APP_KIND_LIGHT) return fprintf(stderr, "light-state: no light window\n"), 0;
        static const char *const COMPARE[] = { "off", "before", "four" };
        const LightUi *ui = &a->light;
        const int mode = ui->compare.mode >= 0 && ui->compare.mode < 3 ? ui->compare.mode : 0;
        return fprintf(stdout, "light state: track %s view %d hud %d compare %s drive %s error: %s\n", light_ui_track(ui->track), ui->view, ui->hud, COMPARE[mode], app_light_drive_running(a) ? "running" : "stopped", ui->error), 1;
    }
    if (strncmp(name, "light-copy:", 11) == 0) {   /* light-copy:KIND,I,TIMES,MODE: Copy to... of the I-th line of KIND (0 brightness, 1 tone, 2 lamp) at the open time, to the times in the bits of TIMES (1 Dawn, 2 Day, 4 Dusk, 8 Night); MODE 0 asks when a line clashes (the window), 1 replaces, 2 leaves */
        static const int KINDS[3] = { LT_BRIGHTNESS, LT_TONE, LT_LAMP };
        if (a->kind != APP_KIND_LIGHT || arg[0] < 0 || arg[0] > 2) return fprintf(stderr, "light-copy: no light window or no such kind\n"), 0;
        return app_light_copy(a, KINDS[(int)arg[0]], (int)arg[1], (unsigned)arg[2], (int)arg[3]);
    }
    if (strncmp(name, "light-copy-answer:", 18) == 0) return a->kind == APP_KIND_LIGHT && app_light_copy_answer(a, (int)arg[0]);   /* light-copy-answer:N the window's answer to a waiting clash: 1 Replace, 2 Skip, 3 Cancel */
    if (strcmp(name, "light-note") == 0) {   /* what the last Copy to... said, and whether a clash is waiting for the window */
        if (a->kind != APP_KIND_LIGHT) return 0;
        return fprintf(stdout, "light note: %s | pending %d | error: %s\n", a->light.copy_note, a->light.copy_pending, a->light.error), 1;
    }
    if (strcmp(name, "light-node") == 0) return a->kind == APP_KIND_LIGHT ? (fprintf(stdout, "light node: %d\n", a->light.node), 1) : 0;   /* the photo's node, as the panel has it */
    if (strcmp(name, "print-repo") == 0) return fprintf(stdout, "repo data: %s\n", app_repo_data(a)[0] != '\0' ? app_repo_data(a) : "none"), 1;   /* where Copy to game repo writes, none when it is hidden */
    if (strncmp(name, "masks-only:", 11) == 0) return a->tmask_masked_only = arg[0] != 0, 1;   /* masks-only:N, the Masks table's «With mask only» switch (1 hides the textures with no mask) */
    if (strcmp(name, "masks-poll") == 0) return app_track_masks_poll(a), 1;   /* the panel's look at the PSDs (once a second by the clock) */
    if (strncmp(name, "mask-select:", 12) == 0) return app_track_masks_load(a) && app_track_mask_select(a, (int)arg[0]);   /* the I-th mask shown over its texture */
    if (strcmp(name, "mask-regen") == 0) return app_track_mask_regenerate(a);   /* Regenerate from PSD on the shown mask */
    if (strncmp(name, "light-bright:", 13) == 0) {   /* light-bright:P: the Add brightness button, its percent P (a change the drive carries to the game) */
        LightTrack *f = &a->light.session.file;
        const unsigned node = (unsigned)a->light.node, last = a->light.session.nodes - 1;
        const LightTrackBrightness b = { node, node + 50 < last ? node + 50 : last, (unsigned)arg[0], LT_ALL_POINTS };
        char why[LT_WHY];
        if (!light_track_set_brightness(f, a->light.time, -1, &b, why, sizeof why)) return fprintf(stderr, "light-bright: %s\n", why), 0;
        return app_light_touch(a), 1;
    }
    if (strncmp(name, "light-tone:", 11) == 0) {   /* light-tone:G: the Add tone button at the open node, the road's strip, a grey of level G (0..255) at 60 % */
        LightTrack *f = &a->light.session.file;
        const unsigned node = (unsigned)a->light.node, last = a->light.session.nodes - 1;
        LightTrackTone t;
        memset(&t, 0, sizeof t);
        t.from = node, t.to = node + 20 < last ? node + 20 : last, t.mask = LT_STRIP_ROAD, t.percent = 60;
        snprintf(t.colour, sizeof t.colour, "%02x%02x%02x", (unsigned)arg[0] & 255, (unsigned)arg[0] & 255, (unsigned)arg[0] & 255);
        char why[LT_WHY];
        if (!light_track_set_tone(f, a->light.time, -1, &t, why, sizeof why)) return fprintf(stderr, "light-tone: %s\n", why), 0;
        return app_light_touch(a), 1;
    }
    if (strncmp(name, "light-lamp:", 11) == 0) {   /* light-lamp:P: the Add lamp button at the open node, its light P percent */
        LightTrack *f = &a->light.session.file;
        LightTrackLamp l;
        memset(&l, 0, sizeof l);
        l.node = (unsigned)a->light.node, l.percent = (unsigned)arg[0];
        char why[LT_WHY];
        if (!light_track_set_lamp(f, a->light.time, -1, &l, why, sizeof why)) return fprintf(stderr, "light-lamp: %s\n", why), 0;
        return app_light_touch(a), 1;
    }
    if (strncmp(name, "light-remove:", 13) == 0) {   /* light-remove:K: the Remove button of the first line of kind K (0 brightness, 1 tone, 2 lamp) of the time shown */
        static const int KIND[3] = { LT_BRIGHTNESS, LT_TONE, LT_LAMP };
        char why[LT_WHY];
        const int kind = (int)arg[0];
        if (kind < 0 || kind > 2 || !light_track_remove(&a->light.session.file, a->light.time, KIND[kind], 0, why, sizeof why)) return fprintf(stderr, "light-remove: %s\n", kind < 0 || kind > 2 ? "no such kind" : why), 0;
        return app_light_touch(a), 1;
    }
    if (strcmp(name, "light-drive") == 0) return app_light_drive(a);   /* the Track light window's Test drive button */
    if (strcmp(name, "drive-stop") == 0) return app_light_drive_stop(a), 1;
    if (strncmp(name, "light-weather:", 14) == 0) return app_light_set_weather(a, (int)arg[0]), 1;   /* light-weather:N: the test drive's weather (0 clear, 1 fog, 2 rain, 3 storm) */
    if (strcmp(name, "print-drive") == 0) {   /* the test drive: its state, track, time, node, weather, the scratch folder's copy of the light file, the status */
        const LightDriveJob *d = &a->light.drive;
        static const char *const STATES[] = { "idle", "running", "ended", "failed" };
        printf("drive: %s %s %c node %u %s\n", STATES[d->state], d->track, d->letter != '\0' ? d->letter : '-', d->node, light_drive_weather_word(d->weather));
        char file[APP_PATH + 80];
        snprintf(file, sizeof file, "%s/track_light/%s.txt", d->mods, d->track);
        FILE *f = d->state == LIGHT_DRIVE_RUNNING ? fopen(file, "rb") : NULL;
        char line[256];
        while (f != NULL && fgets(line, sizeof line, f) != NULL) {
            const char *body = line + strspn(line, " ");   /* the lines of a time's block are indented */
            if (strncmp(body, "brightness", 10) == 0) printf("drive copy: %s", body);
        }
        if (f != NULL) fclose(f);
        printf("drive why: %s\n", d->why);
        printf("status: %s\n", a->status);
        return 1;
    }
    if (strcmp(name, "repo-ask") == 0) return app_original_copy_ask(a);   /* File > Copy to game repo: the preview in publish_text */
    if (strcmp(name, "repo-copy") == 0) return app_original_copy(a);          /* its Copy button */
    if (strcmp(name, "print-publish") == 0) return fprintf(stdout, "publish: %s for %s\n%s\n", a->publish_dialog ? "window up" : "no window", a->publish_car ? "a car" : "a track", a->publish_text), 1;   /* the Copy to game repo window and its list */
    if (strcmp(name, "print-original") == 0) {   /* the original track open (or none), the light folder, the track and time, the publish preview on one line, the status */
        char text[sizeof a->publish_text];
        snprintf(text, sizeof text, "%s", a->publish_text);
        for (char *p = text; *p != '\0'; ++p) if (*p == '\n') *p = ' ';
        fprintf(stdout, "original: %s\nlight: %s\n", a->original_dir[0] != '\0' ? a->original_dir : "none", a->light_dir);
        if (a->kind == APP_KIND_LIGHT) fprintf(stdout, "track: %s time: %c\n", light_ui_track(a->light.track), light_ui_time_letter(a->light.time));
        return fprintf(stdout, "preview: %s\nstatus: %s\n", text, a->status), 1;
    }
    if (strcmp(name, "print-menu") == 0) return ui_print_menu(a), 1;   /* the room on screen and the File menu's items in it */
    if (strcmp(name, "main-menu") == 0) return app_close_project(a);   /* File > Main menu (a headless run drops the question about unsaved changes) */
    if (strcmp(name, "print-panels") == 0) return ui_print_panels(a), 1;   /* the Window menu's panels, open or closed, drawn or not, and the ones drawn that it does not list */
    if (strcmp(name, "reset-layout") == 0) return ui_reset_layout(), 1;   /* Window > Reset layout, as the menu does it (a few frames later the dock is rebuilt) */
    if (strncmp(name, "psd-wait:", 9) == 0) return 2;   /* by_frames: N frames for a Convert PSD to run, then its words */
    if (strncmp(name, "poll-wait:", 10) == 0) return 2;   /* by_frames: the window runs S seconds, the panel's poll looking at the PSDs as it does when open */
    if (strncmp(name, "ui-", 3) == 0) return 2;   /* by_frames: the mouse goes in on a widget a panel named (ui_probe.h) */
    if (strncmp(name, "scan:", 5) == 0) return 2;   /* by_frames: the mouse over the window, a point a frame */
    if (strncmp(name, "panel:", 6) == 0) return 2;   /* by_frames: a click on the I-th item of the Window menu, drawn by the next frame */
    if (strcmp(name, "print-view") == 0) return fprintf(stdout, "view: %.1f %.1f %.2f\n", a->view.cx, a->view.cz, a->view.scale), 1;   /* the top view's centre (x z) and pixels per metre */
    if (strncmp(name, "slope-preview:", 14) == 0) return a->kind == APP_KIND_TRACK ? (app_slope_defaults(a), a->slope.preview_hold = arg[0] != 0, a->slope.grade_pct = arg[0] > 1 ? arg[0] : a->slope.grade_pct, a->slope.ease_in_m = arg[0] > 1 ? 0 : a->slope.ease_in_m, a->slope.ease_out_m = arg[0] > 1 ? 0 : a->slope.ease_out_m, 1) : 0;   /* the profile draws what Accept would make, held (for a picture) */
    if (strncmp(name, "slope", 5) == 0) {   /* slope:G,IN,OUT,KEEP / slope-rise:R,IN,OUT,KEEP: the Slope block's fields set, then Accept */
        if (a->kind != APP_KIND_TRACK) return 0;
        app_slope_defaults(a);
        a->slope.by_rise = name[5] == '-';
        if (a->slope.by_rise) a->slope.rise_m = arg[0];
        else a->slope.grade_pct = arg[0];
        a->slope.ease_in_m = arg[1], a->slope.ease_out_m = arg[2], a->slope.keep_after = arg[3] != 0;
        app_slope_accept(a);   /* a refusal is in the status, not a failed step */
        return 1;
    }
    if (strncmp(name, "strip-split:", 12) == 0 || strncmp(name, "strip-size:", 11) == 0) return 2;   /* by_frames: the mouse goes in */
    if (strcmp(name, "print-strip") == 0) return a->strip_win.drawn ? (fprintf(stdout, "strip: %.0f profile: %.0f\n", a->strip_win.h, a->profile_drawn.drawn ? fminf(a->profile_drawn.bottom + 4, a->strip_win.y + a->strip_win.h) - (a->profile_drawn.top - 4) : 0.0f), 1) : 0;   /* the Distance strip window's height and its curve's part in sight, in pixels, as drawn last frame */
    if (strncmp(name, "profile-click:", 14) == 0) return 2;   /* by_frames: the mouse goes in */
    if (strcmp(name, "insert-here") == 0) return app_insert_here(a), 1;   /* the Insert point here button */
    if (strcmp(name, "print-workspace") == 0) {   /* the workspace, whether the start screen offers to choose one, and the projects the workspace lists */
        fprintf(stdout, "workspace: %s\noffer: %d\ncars:", a->workspace[0] != '\0' ? a->workspace : "none", app_workspace_offer(a));
        for (int i = 0; i < a->workspace_list.cars; ++i) fprintf(stdout, " %s", a->workspace_list.car[i]);
        fprintf(stdout, "\ntracks:");
        for (int i = 0; i < a->workspace_list.tracks; ++i) fprintf(stdout, " %s", a->workspace_list.track[i]);
        return fprintf(stdout, "\n"), 1;
    }
    if (strncmp(name, "open-ws:", 8) == 0) {   /* open-ws:K,I: the workspace's I-th car (K 0) or track (K 1) opened, as a click on its row on the start screen does (a headless run drops the question about unsaved changes) */
        char path[APP_PATH];
        return app_workspace_item(a, (int)arg[0], (int)arg[1], path, sizeof path) && (arg[0] == 0 ? app_open_car(a, path) : app_open_project(a, path));
    }
    if (strncmp(name, "dialog-dir:", 11) == 0) {   /* dialog-dir:K: where the folder dialog starts, K 0 for New/Open track, 1 for Open car, 2 for the workspace's own */
        char where[APP_PATH];
        const int kind = (int)arg[0];
        app_dialog_dir(a, kind == 0 ? APP_ACT_OPEN : kind == 1 ? APP_ACT_OPEN_CAR : APP_ACT_WORKSPACE_PICK, where, sizeof where);
        return fprintf(stdout, "dialog: %s\n", where), 1;
    }
    if (strcmp(name, "no-workspace") == 0) return app_workspace_clear(a), 1;   /* File > Workspace > No workspace */
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
    if (strncmp(name, "night-light:", 12) == 0) {   /* night-light:N, night-tint:R,G,B, night-reset:K (0 the light, 1 the tint), print-night: the Preview's Night light and tint, as its slider, picker and Reset buttons set them */
        a->night.light = (int)arg[0] < NIGHT_LIGHT_MIN ? NIGHT_LIGHT_MIN : (int)arg[0] > NIGHT_LIGHT_MAX ? NIGHT_LIGHT_MAX : (int)arg[0];
        return 1;
    }
    if (strncmp(name, "night-tint:", 11) == 0) {
        for (int i = 0; i < 3; ++i) a->night.tint[i] = arg[i] < 0 ? 0.0f : arg[i] > 1 ? 1.0f : (float)arg[i];
        return 1;
    }
    if (strncmp(name, "night-reset:", 12) == 0) {
        if (arg[0] == 0) night_settings_reset_light(&a->night);
        else night_settings_reset_tint(&a->night);
        return 1;
    }
    if (strcmp(name, "lights-open") == 0) return a->lights_open = 1, 1;   /* the car Preview's Lights header unfolded by the next frame */
    if (strcmp(name, "print-confirm") == 0)   /* what is waiting for an answer ("confirm: quit"), what is to run ("go: quit") or "confirm: none" / "go: none"; Escape and a clean quit are checked with it */
        return fprintf(stdout, "confirm: %s\ngo: %s\n", a->confirm == APP_ACT_QUIT ? "quit" : a->confirm == APP_ACT_NONE ? "none" : "other", a->go == APP_ACT_QUIT ? "quit" : a->go == APP_ACT_NONE ? "none" : "other"), 1;
    if (strcmp(name, "print-night") == 0) return fprintf(stdout, "night: light=%d tint=%.3f,%.3f,%.3f\n", a->night.light, a->night.tint[0], a->night.tint[1], a->night.tint[2]), 1;
    if (strncmp(name, "print-height", 12) == 0) {   /* "height: I H": point I's height in metres */
        const int point = (int)arg[0];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
        return fprintf(stdout, "height: %d %.1f\n", point, a->trace.points[point].h), 1;
    }
    if (strncmp(name, "height", 6) == 0) {   /* a point's height, as the Points window's h cell and the profile in the Distance strip set it */
        const int point = (int)arg[0];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
        app_set_point_height(a, point, arg[1]);
        return 1;
    }
    if (strncmp(name, "lanes:", 6) == 0) {   /* lanes:P,L: the lanes row's combo on point P, L 0 1+1, 1 1+2, 2 2+1 */
        const int point = (int)arg[0], lanes = (int)arg[1];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || lanes < 0 || lanes > 2) return 0;
        app_set_point_lanes(a, point, lanes == 0 ? TRACK_GEN_LANES_1_1 : lanes == 1 ? TRACK_GEN_LANES_1_2 : TRACK_GEN_LANES_2_1);
        app_track_commit(a);   /* one step to undo, as the window's let go is */
        return 1;
    }
    if (strncmp(name, "extend:", 7) == 0 || strncmp(name, "extend-end:", 11) == 0) {   /* extend:P,F / extend-end:P,F: right-click Extend to next change / to the end; F 0 width, 1 bank, 2 lanes */
        const int point = (int)arg[0], field = (int)arg[1];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || field < 0 || field > 2) return 0;
        app_extend_point(a, point, field == 0 ? TRACE_KEY_WIDTH : field == 1 ? TRACE_KEY_BANK : TRACE_KEY_LANES, name[6] == '-');
        app_track_commit(a);
        return 1;
    }
    if (strcmp(name, "print-road") == 0) {   /* what the keys make at each point: road P: width bank lanes shoulder-left|right surface (1 verge, 2 tarmac) */
        if (a->kind != APP_KIND_TRACK) return 0;
        KeyState st;
        if (!key_state_make(&a->trace, &st)) return 0;
        for (int i = 0; i < a->trace.count; ++i)
            fprintf(stdout, "road %d: width %.2f bank %.1f lanes %d shoulder %.2f|%.2f surface %d|%d\n", i, st.width_m[i], st.bank_deg[i], st.lanes[i],
                    st.side[i][TRACE_SIDE_SHOULDER][TRACK_GEN_LEFT], st.side[i][TRACE_SIDE_SHOULDER][TRACK_GEN_RIGHT], (int)st.surface[0][i], (int)st.surface[1][i]);
        key_state_free(&st);
        return 1;
    }
    if (strncmp(name, "side:", 5) == 0) {   /* side:P,F,S,V: a Road profile side cell on point P (F TRACE_SIDE_ 0 shoulder, 1 barrier, 2 wall, 3 fence; S 0 left, 1 right) set to V */
        const int point = (int)arg[0], field = (int)arg[1], side = (int)arg[2];
        if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || field < 0 || field >= TRACE_SIDE_FIELDS || side < 0 || side > 1) return 0;
        app_set_point_side(a, point, field, side, arg[3]);
        app_track_commit(a);
        return 1;
    }
    if (strcmp(name, "print-mixed") == 0) {   /* the values the selected points do not share, which the Road profile shows as a dash */
        static const char *const FIELD[TRACE_SIDE_FIELDS] = { "shoulder", "barrier", "wall", "fence" };
        if (a->kind != APP_KIND_TRACK) return 0;
        const unsigned m = app_sel_mixed(a);
        fprintf(stdout, "mixed:");
        if (m & TRACE_KEY_WIDTH) fprintf(stdout, " width");
        if (m & TRACE_KEY_BANK) fprintf(stdout, " bank");
        if (m & TRACE_KEY_LANES) fprintf(stdout, " lanes");
        if (m & TRACE_KEY_FENCE_HEIGHT) fprintf(stdout, " fence-height");
        if (m & TRACE_KEY_FENCE_ART) fprintf(stdout, " fence-art");
        for (int side = 0; side < 2; ++side) {
            if (m & APP_SEL_MIXED_SURFACE(side)) fprintf(stdout, " surface-%c", side ? 'r' : 'l');
            if (m & TRACE_KEY_BARRIER_CUSTOM(side)) fprintf(stdout, " custom-%c", side ? 'r' : 'l');
            for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
                if (m & TRACE_KEY_SIDE(f, side)) fprintf(stdout, " %s-%c", FIELD[f], side ? 'r' : 'l');
        }
        fprintf(stdout, "\n");
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
    if (strncmp(name, "camera:", 7) == 0) {   /* camera:2: the car read again, then two frames for the Model to answer */
        if (p == 0) app_car_reread(a);
        return p < 2;
    }
    if (strncmp(name, "psd-wait:", 9) == 0) {   /* psd-wait:N: ends when a Convert PSD has run and has its words, at most N frames and at most PSD_WAIT_SECONDS of the clock (a window at rest draws twice a second: N frames can be minutes); then "psd: busy|idle: <the last Convert PSD's words>" (the status line is the last thing said, so it is not enough) */
        static Uint64 from;
        enum { PSD_WAIT_SECONDS = 120 };
        if (p == 0) from = SDL_GetTicks();
        const int done = p >= 2 && a->psd.text[0] != '\0' && !app_psd_busy(a);
        const int late = (SDL_GetTicks() - from) / 1000 >= PSD_WAIT_SECONDS;
        if (p + 1 < (int)arg[0] && !done && !late) return SDL_Delay(5), 1;
        if (late && !done) fprintf(stdout, "psd: no end after %d seconds (frame %d)\n", PSD_WAIT_SECONDS, p);
        fprintf(stdout, "psd: %s: %s\n", app_psd_busy(a) ? "busy" : "idle", a->psd.text), fflush(stdout);
        return 0;
    }
    if (strncmp(name, "poll-wait:", 10) == 0) {   /* poll-wait:S: S seconds of frames; "poll: watching" at the start, "poll: STATUS" each time the photos are due again (a PSD saved meanwhile by the test) */
        if (p == 0) s->wait_from = a->kind == APP_KIND_LIGHT ? a->light.now : a->tmask_now, s->wait_serial = a->light.edit_serial, fprintf(stdout, "poll: watching\n"), fflush(stdout);
        if (a->light.edit_serial != s->wait_serial) s->wait_serial = a->light.edit_serial, fprintf(stdout, "poll: %s\n", a->status), fflush(stdout);
        return (a->kind == APP_KIND_LIGHT ? a->light.now : a->tmask_now) - s->wait_from < arg[0];
    }
    if (strcmp(name, "curve-block") == 0) {   /* the block as the panel drew it in the frame before */
        fprintf(stdout, "curve block: %s\n", a->curve.block_open == 1 ? "open" : a->curve.block_open == 2 ? "folded" : "not drawn");
        return 0;
    }
    if (strncmp(name, "scan:", 5) == 0) {   /* scan:DX,DY: the mouse over the window's left half, a point every DX by DY pixels, a frame each: the ids ImGui flags as shared (visible items only) show, and the run fails */
        int w = 0, h = 0;
        SDL_GetWindowSize(win, &w, &h);
        const int dx = arg[0] >= 4 ? (int)arg[0] : 4, dy = arg[1] >= 4 ? (int)arg[1] : 4, cols = w / 2 / dx + 1, rows = h / dy + 1;
        if (p >= cols * rows) return 0;
        mouse(win, SDL_EVENT_MOUSE_MOTION, (float)((p % cols) * dx), (float)((p / cols) * dy));
        return 1;
    }
    if (strncmp(name, "panel:", 6) == 0) {   /* panel:I: the Window menu's I-th item clicked: closed when open, open when closed */
        if (p == 0 && !ui_panel_toggle(a, (int)arg[0])) return s->failed = 1, 0;
        return p == 0;
    }
    if (strncmp(name, "curve-fold:", 11) == 0) {   /* a click on the header: set now, drawn by the next frame, which this step waits for */
        if (p == 0) a->curve.block_force = arg[0] != 0 ? 1 : 2;
        return p == 0;
    }
    if (strncmp(name, "dblclick-road:", 14) == 0 || strncmp(name, "shift-dblclick:", 15) == 0 || strncmp(name, "cmd-dblclick:", 13) == 0) {   /* dblclick-road:X,Z: a double click on the road where track place (X, Z) is, in metres; shift-dblclick, cmd-dblclick: with the modifier held */
        SDL_Keycode mod_key;
        SDL_Keymod mod;
        const char *rest;
        held(name, &mod_key, &mod, &rest);
        if (p == 0 && mod_key != SDLK_UNKNOWN) key_event(win, mod_key, 1, mod);
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
        if (p == 5 && mod_key != SDLK_UNKNOWN) key_event(win, mod_key, 0, SDL_KMOD_NONE);
        return p < 6;
    }
    if (strncmp(name, "strip-split:", 12) == 0 || strncmp(name, "strip-size:", 11) == 0) {   /* strip-split:DY the splitter between the Viewport and the Distance strip (the strip's top edge) dragged DY pixels (down is a lower strip) */
        if (p == 0) {
            if (!a->strip_win.drawn) return s->failed = 1, 0;
            s->from_x = a->strip_win.x + a->strip_win.w / 2, s->from_y = a->strip_win.y - 1, s->to_x = s->from_x, s->to_y = s->from_y + (name[7] == 'i' ? a->strip_win.h - (float)arg[0] : (float)arg[0]);   /* strip-size:H: to H pixels high, whatever it was */
            mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y);
        } else if (p == 1) {
            mouse(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y);
        } else if (p <= 1 + DRAG_MOVES) {
            const float f = (float)(p - 1) / DRAG_MOVES;
            mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y + f * (s->to_y - s->from_y));
        } else if (p == 2 + DRAG_MOVES) {
            mouse(win, SDL_EVENT_MOUSE_BUTTON_UP, s->to_x, s->to_y);
        }
        return p < 3 + DRAG_MOVES;
    }
    if (strncmp(name, "profile-click:", 14) == 0) {   /* profile-click:I,DX: a click on the profile DX pixels right of point I's mark (DX past the mark's reach: a place between two points) */
        if (p == 0 && !mark_at(a, (int)arg[0], &s->from_x, &s->from_y)) return s->failed = 1, 0;
        if (p == 0) s->from_x += (float)arg[1];
        if (p == 0) mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y);
        if (p == 1) mouse(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y);
        if (p == 2) mouse(win, SDL_EVENT_MOUSE_BUTTON_UP, s->from_x, s->from_y);
        return p < 3;
    }
    if (strncmp(name, "strip-dblclick:", 15) == 0) {   /* strip-dblclick:I: a double click on point I's mark in the strip's profile */
        if (p == 0 && !mark_at(a, (int)arg[0], &s->from_x, &s->from_y)) return s->failed = 1, 0;
        if (p == 0) mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y);
        if (p == 1) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y, 1);
        if (p == 2) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_UP, s->from_x, s->from_y, 1);
        if (p == 3) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y, 2);
        if (p == 4) mouse_clicks(win, SDL_EVENT_MOUSE_BUTTON_UP, s->from_x, s->from_y, 2);
        return p < 5;
    }
    if (strcmp(name, "sheet") == 0) {   /* sheet: the car's sheet started, then waited for (a sheet is a few photos of a game run) */
        if (p == 0 && !app_sheet_start(a)) return s->failed = 1, 0;
        if (p >= SHEET_FRAMES) return fprintf(stderr, "--steps sheet: it did not end\n"), s->failed = 1, 0;
        return app_sheet_busy(a);
    }
    if (strncmp(name, "ui-focus:", 9) == 0) {   /* ui-focus:WINDOW the docked window brought to the front (its panel draws from the next frame; the step waits for it) */
        if (p == 0) ui_probe_focus(name + 9);
        return p < 2;
    }
    if (strncmp(name, "ui-text:", 8) == 0) {   /* ui-text:TEXT typed into the box that has the focus (a click on it first; Backspace keys clear what it holds), as the keyboard's text events */
        if (p == 0) {
            SDL_Event ev;
            memset(&ev, 0, sizeof ev);
            ev.type = SDL_EVENT_TEXT_INPUT;
            ev.text.windowID = SDL_GetWindowID(win);
            ev.text.text = name + 8;
            SDL_PushEvent(&ev);
        }
        return p < 2;
    }
    if (strncmp(name, "ui-rect:", 8) == 0) {   /* ui-rect:W prints where the widget W is drawn, in window pixels: `ui rect: W x y w h` (a test of the layout: a picture that fits the panel) */
        float rx, ry, rw, rh;
        if (!ui_probe_find(name + 8, &rx, &ry, &rw, &rh)) {
            if (p == 0) fprintf(stderr, "--steps %s: no such widget drawn\n", name);
            return s->failed = 1, 0;
        }
        if (p == 0) fprintf(stdout, "ui rect: %s %.0f %.0f %.0f %.0f\n", name + 8, rx, ry, rw, rh);
        return p < 1;
    }
    if (strncmp(name, "ui-", 3) == 0) {   /* ui-click:W the widget W clicked in its middle; ui-at:W,F a click F (0..1) of its width along (a slider, a bar's cell); ui-span:W,F0,F1 pressed at F0, dragged, let go at F1 */
        const char *colon = strchr(name, ':');
        float wx, wy, ww, wh;
        if (colon == NULL || !ui_probe_find(colon + 1, &wx, &wy, &ww, &wh)) {
            if (p == 0) fprintf(stderr, "--steps %s: no such widget drawn\n", name);
            return s->failed = 1, 0;
        }
        const int span = strncmp(name, "ui-span", 7) == 0;
        const double f0 = strncmp(name, "ui-click", 8) == 0 ? 0.5 : arg[0], f1 = span ? arg[1] : f0;
        const float y = wy + wh * 0.5f, x0 = wx + (float)f0 * ww, x1 = wx + (float)f1 * ww;
        if (p == 0) mouse(win, SDL_EVENT_MOUSE_MOTION, x0, y);
        else if (p == 1) mouse(win, SDL_EVENT_MOUSE_BUTTON_DOWN, x0, y);
        else if (span && p <= 1 + DRAG_MOVES) mouse(win, SDL_EVENT_MOUSE_MOTION, x0 + (float)(p - 1) / DRAG_MOVES * (x1 - x0), y);
        else if (p == (span ? 2 + DRAG_MOVES : 2)) mouse(win, SDL_EVENT_MOUSE_BUTTON_UP, x1, y);
        return p < (span ? 3 + DRAG_MOVES : 3);
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
    if (strcmp(name, "strip") == 0) {   /* the Distance strip's tab to the front: set now, drawn by the next frame, which this step waits for */
        if (p == 0) a->focus_strip = 1;
        return p < 2;
    }
    /* drag:I,DX,DY: to the point, down, DRAG_MOVES moves, up; drag-near:I,J,M the same to M metres before point J on the way from I */
    if (p == 0) {
        const int profile = strncmp(name, "profile-", 8) == 0;   /* profile-drag:I,DY: point I's mark in the strip's profile, dragged DY pixels (down is lower) */
        if (!(profile ? mark_at(a, (int)arg[0], &s->from_x, &s->from_y) : point_at(a, (int)arg[0], &s->from_x, &s->from_y))) return s->failed = 1, 0;
        if (profile) s->to_x = s->from_x, s->to_y = s->from_y + (float)arg[1];
        else if (strncmp(name, "drag-near", 9) == 0 && !near_point(a, (int)arg[0], (int)arg[1], arg[2], s)) return s->failed = 1, 0;
        if (!profile && strncmp(name, "drag-near", 9) != 0) s->to_x = s->from_x + (float)arg[1], s->to_y = s->from_y + (float)arg[2];
        mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x, s->from_y);
    } else if (p == 1) {
        mouse(win, SDL_EVENT_MOUSE_BUTTON_DOWN, s->from_x, s->from_y);
    } else if (p <= 1 + DRAG_MOVES) {
        if (p == 2 && strncmp(name, "profile-fine", 12) == 0) key_event(win, SDLK_LSHIFT, 1, SDL_KMOD_LSHIFT);   /* profile-fine:I,DY: the same with Shift held during the drag (a tenth as far) */
        const float f = (float)(p - 1) / DRAG_MOVES;
        mouse(win, SDL_EVENT_MOUSE_MOTION, s->from_x + f * (s->to_x - s->from_x), s->from_y + f * (s->to_y - s->from_y));
    } else if (p == 2 + DRAG_MOVES) {
        mouse(win, SDL_EVENT_MOUSE_BUTTON_UP, s->to_x, s->to_y);
        if (strncmp(name, "profile-fine", 12) == 0) key_event(win, SDLK_LSHIFT, 0, SDL_KMOD_NONE);
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
