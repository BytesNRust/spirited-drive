#include "steps.h"
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

static const char *const NAMES[] = { "edit", "undo", "redo", "save", "title", "print", "key", "click", "drag", "width", "race", "race-log", "race-stop", "bank", "drag-near", "problems", "build",
                                     "shift-click", "cmd-click", "box", "row", "shift-row", "cmd-row", "rotate", "print-sel", "print-profile", "status", "copy", "paste", "mirror-lr", "mirror-fb", "reverse", "curve", "straight", "preview", "curve-block", "curve-fold", "colour-by", "dblclick-road", "print-classes", "sheet", "print-sheet", "sheet-save", "height", "strip", "profile-drag", "print-height", "strip-dblclick", "print-view", "profile-fine", "profile-click", "insert-here", "slope", "slope-rise", "slope-preview", "shift-dblclick", "cmd-dblclick", "lanes", "extend", "extend-end", "print-road", "copy-profile", "paste-profile", "side", "print-mixed", "print-workspace", "open-ws", "dialog-dir", "no-workspace", "panel", "print-panels", "print-menu", "main-menu", "new-track", "new-track-open", "strip-split", "print-strip", "strip-size", "night-light", "night-tint", "night-reset", "print-night", "lights-open", "card-photo", "card-view", "card-backdrop", "card-bd-bottom", "ui-fault", "scan", "add-originals", "open-original", "publish-ask", "publish", "print-original", "original-open", "masks-print", "mask-select", "mask-regen", "check-originals", "masks-poll", "repo-ask", "repo-copy", "print-repo", "print-confirm", "poll-wait", "check", "new-car-open", "new-car", "physics-from", "physics-open", "export-open", "export-track" };
static const int ARGS[] = { 0, 0, 0, 0, 0, 0, 0, 1, 3, 2, 0, 0, 0, 2, 3, 0, 0, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 1, 0, 1, 1, 2, 0, 1, 0, 0, 2, 0, 2, 1, 1, 0, 2, 2, 0, 4, 4, 1, 2, 2, 2, 2, 2, 0, 0, 0, 4, 0, 0, 2, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 3, 1, 0, 0, 0, 2, 4, 3, 0, 2, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1 };   /* the numbers each takes (key takes its name after the colon) */

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
    if (strncmp(name, "physics-open:", 13) == 0) {   /* physics-open:T: the question up with car T named (for a picture) */
        a->physics_pick = (int)arg[0], a->physics_dialog = 1;
        return 1;
    }
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
    if (strcmp(name, "print-repo") == 0) return fprintf(stdout, "repo data: %s\n", app_repo_data(a)[0] != '\0' ? app_repo_data(a) : "none"), 1;   /* where Copy to game repo writes, none when it is hidden */
    if (strcmp(name, "masks-poll") == 0) return app_track_masks_poll(a), 1;   /* the panel's look at the PSDs (once a second by the clock) */
    if (strncmp(name, "mask-select:", 12) == 0) return app_track_masks_load(a) && app_track_mask_select(a, (int)arg[0]);   /* the I-th mask shown over its texture */
    if (strcmp(name, "mask-regen") == 0) return app_track_mask_regenerate(a);   /* Regenerate from PSD on the shown mask */
    if (strcmp(name, "repo-ask") == 0) return app_original_copy_ask(a);   /* File > Copy to game repo: the preview in publish_text */
    if (strcmp(name, "repo-copy") == 0) return app_original_copy(a);          /* its Copy button */
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
    if (strncmp(name, "poll-wait:", 10) == 0) return 2;   /* by_frames: the window runs S seconds, the panel's poll looking at the PSDs as it does when open */
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
    if (strncmp(name, "poll-wait:", 10) == 0) {   /* poll-wait:S: S seconds of frames; "poll: watching" at the start, "poll: STATUS" each time the photos are due again (a PSD saved meanwhile by the test) */
        if (p == 0) s->wait_from = a->light.now, s->wait_serial = a->light.edit_serial, fprintf(stdout, "poll: watching\n"), fflush(stdout);
        if (a->light.edit_serial != s->wait_serial) s->wait_serial = a->light.edit_serial, fprintf(stdout, "poll: %s\n", a->status), fflush(stdout);
        return a->light.now - s->wait_from < arg[0];
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
