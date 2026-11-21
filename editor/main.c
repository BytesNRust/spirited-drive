/* tnfs-track-editor: Spirited Drive Studio's window. A top view of a track project's centre line (phase 1a, notes/dep59-track-editor-design.md), or a
 * car project's card, textures and masks (dep59 car editor 1d, read only for now); with nothing open, a start screen.
 *
 *   tnfs-track-editor [PROJECT] [--game PATH] [--user DIR]
 *                     [--frames N] [--save] [--build] [--install] [--select N] [--orbit YAW,PITCH[,DIST]] [--shine on|off] [--wheels still|slow|fast] [--edit] [--steps STEP,...] [--set KEY=VALUE] [--ask quit|new|open|opencar] [--shot OUT.png]
 *
 * PROJECT is a folder such as <workspace>/tracks/<id>/ (created with its project.ini and trace.json when missing), or a car's kit folder such as
 * <workspace>/cars/<kit>/ (the workspace: one folder with cars/ and tracks/, --workspace or File > Workspace): a folder with a project.ini is a track, one with a car's mod.ini is a car. The build uses no CD data:
 * the road is made on the generic template and the stage's art is the FAM named by project.ini's art=. --game and --user are for the
 * install (the game's --install; without --user, into the game's own player folder). Without PROJECT the editor reopens the project it had last, or shows
 * the start screen; without --game it takes the Spirited Drive app beside it (inside the DMG) or ./tnfs. The options after --frames run headless: N frames, then they are done, and --shot writes the last frame to a PNG; the
 * exit status is 1 when a --save, --build or --install failed. Run it with SDL_VIDEO_DRIVER=dummy and SDL_AUDIO_DRIVER=dummy for a headless
 * run. */
#include "app.h"
#include "app_select.h"
#include "app_dash.h"
#include "app_sheet.h"
#include "art.h"
#include "build.h"
#include "capture.h"
#include "game_find.h"
#include "mask_changes.h"
#include "formats/png.h"
#include "steps.h"
#include "ui.h"
#include "window_fit.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <spawn.h>

extern char **environ;

enum {
    WINDOW_W = WINDOW_LEAST_W,   /* a headless run's window (--frames, --shot): the size the tests' pictures are of */
    WINDOW_H = WINDOW_LEAST_H,
    WINDOW_MIN_W = 640, WINDOW_MIN_H = 400, WINDOW_MAX_W = 3840, WINDOW_MAX_H = 2160,   /* --window's range */
    FRAME_MS = 16,
    LIGHT_WAIT_FRAMES = 20000,   /* a headless Track light run waits this many frames at most for its pictures (5 minutes) */
    TEST_DRIVE_WAIT_MS = 60000   /* --test-drive waits this long for the game's line on the track */
};

#ifndef STUDIO_LIGHT_DIR
#define STUDIO_LIGHT_DIR ""   /* a build with no source tree to tune: --light-dir names the folder */
#endif

typedef struct {
    const char *dir, *game, *user, *shot;
    const char *psd_root;     /* --psd-root DIR: the folder of the owner's PSDs, one folder a car (Convert PSD) */
    const char *wheels;       /* --wheels still|slow|fast: the Model window's Wheels before the first frame (a headless picture shows Slow's first frame) */
    int frames, save, build, install, select;   /* select: a control point to select before the first frame, -1 for none */
    int test_drive;           /* --test-drive: Test drive as the Drive window runs it, until the game's log says whether it took the track; then the game is stopped */
    const char *set[8];      /* --set key=value, a car's field set before the first frame (a headless picture of a problem, a save of a change) */
    int set_count;
    const char *add_photo[8];   /* --add-photo FILE.png (up to 8): a showcase photo added to the open car before the first frame, as Add photo... does */
    int add_photo_count;
    const char *light_pick;  /* --light TRACK:TIME:NODE (CY1:N:70): the Track light window's first track, time of day (A, B, C or N) and photo node, for a headless picture or flow */
    const char *light_compare;   /* --compare off|before|four: what the Track light window shows beside the photo (part D) */
    const char *light_dir;   /* --light-dir DIR: the folder whose track_light/ the Track light window tunes (default: the source tree's data/) */
    const char *pick;        /* --pick FOLDER: the car chosen in the Export window before the first frame */
    const char *workspace;   /* --workspace DIR: the workspace folder (cars/ and tracks/ in it) for this run, kept like one chosen in the menu */
    const char *export_car, *export_into, *cd, *port_data;   /* --export-car STEM --into DIR, --cd DIR, --port-data DIR: Export original car without the window's dialogs */
    int convert_psd;          /* --convert-psd: Convert PSD before the first frame, waited for (a headless run); its exit status says how it ended */
    int photo_view;           /* --photo-view (with --race-photo): the photo's large window open for the --shot */
    int race_photo, night;    /* --race-photo: take the race photo before the first frame (a headless run waits for it); --night: the Night preset */
    const char *track, *race_png;   /* --track STEM, --race-png FILE: the track and where the photo is also written */
    const char *race_off;   /* --race-off reflections,car_colours,soft_shadow,texture_filter: the Race look's switches off, for a headless photo */
    int race_scale;         /* --race-scale 1|2: its render scale (0: the default) */
    int race_cockpit;       /* --race-view cockpit: a photo from the dash (follow, the default, is 0) */
    int gauge_light;        /* --gauge-light 0..5 (a cockpit photo's night dash), 0 for none given; stored as the value + 1 */
    const char *card_backdrop;   /* --card-backdrop FILE: the card photo's backdrop is that PNG */
    const char *orbit;       /* --orbit YAW,PITCH[,DIST]: the Model window's camera, in degrees (a headless picture of a view) */
    const char *shine;       /* --shine on|off: the Model window's body shine (on by default) */
    const char *mask;        /* --mask NAME: the mask (topv, frnt_lo) opened in the mask view before the first frame */
    const char *view, *lights, *night_tint, *preview_tex;   int night_light;   /* --view day|night, --lights head,side,brake,reverse,police, --night-light PERCENT, --night-tint R,G,B (0..1): the Preview's Day / Night and its lamps, for a headless picture */
    int changes;             /* --changes: the project's masks compared with the game's, the list printed (what Copy the changed list copies) */
    int preview_paint;       /* --paint N: the card's N-th paint (from 1) shown in Preview, for a headless picture; 0 the car as its file has it */
    int edit;                /* a point added before the first frame: unsaved changes, for a headless picture of the confirmation */
    const char *steps;       /* --steps ...: a user's edits played through the window in order (steps.h), for a flow test */
    AppAction ask;           /* an action asked for before the first frame (quit, new, open) */
    int reset_layout;        /* --reset-layout: Window > Reset layout before the first frame (a headless check that it gives the first-run dock back) */
    int window_w, window_h;  /* --window WxH: the window's size (a headless picture of a layout at another size); 0 x 0 for the default: WINDOW_W x WINDOW_H
                              * headless, what the display leaves on a normal run (window_fit) */
} Options;

static int usage(void)
{
    fprintf(stderr, "usage: tnfs-track-editor [PROJECT] [--game PATH] [--user DIR] [--frames N] [--save] [--build] "
                    "[--install] [--select N] [--edit] [--set KEY=VALUE] [--ask quit|new|open|opencar|export] [--shot OUT.png]\n"
                    "       [--export-car FOLDER [--into PARENT]] [--cd DIR] [--port-data DIR] [--workspace DIR] [--pick FOLDER] [--convert-psd] [--psd-root DIR] [--race-photo [--photo-view] [--night] [--track STEM] [--race-png OUT.png] [--race-off reflections,car_colours,soft_shadow,texture_filter] [--race-scale 1|2] [--race-view follow|cockpit] [--gauge-light 0..5]]\n"
                    "       [--mask NAME] [--paint N] [--view day|night] [--lights head,side,brake,reverse,police] [--night-light PERCENT] [--night-tint R,G,B] [--preview-tex NAME] [--changes] [--card-backdrop FILE.png] [--add-photo FILE.png]...\n"
                    "       [--test-drive [--race-off ...] [--race-scale 1|2]] [--wheels still|slow|fast] [--window WxH] [--reset-layout] [--light-dir DIR] [--ask light [--light TRACK:TIME:NODE] [--compare off|before|four]] [--steps STEP,...]\n");
    return 0;
}

/* --race-off's comma list onto a look: 0 when a word is not a switch, or the list is too long to read whole */
static int race_off_apply(const char *list, RaceLook *look)
{
    char words[128];
    const int n = snprintf(words, sizeof words, "%s", list);
    if (n < 0 || (size_t)n >= sizeof words) {
        fprintf(stderr, "--race-off: the list is longer than %zu bytes\n", sizeof words - 1);
        return 0;
    }
    int ok = 1;
    for (char *w = strtok(words, ","); w != NULL; w = strtok(NULL, ",")) {
        if (strcmp(w, "reflections") == 0) look->reflections = 0;
        else if (strcmp(w, "car_colours") == 0) look->car_colours = 0;
        else if (strcmp(w, "soft_shadow") == 0) look->soft_shadow = 0;
        else if (strcmp(w, "texture_filter") == 0) look->texture_filter = 0;
        else {
            fprintf(stderr, "--race-off '%s': reflections, car_colours, soft_shadow or texture_filter (commas, no spaces)\n", w);
            ok = 0;
        }
    }
    return ok;
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    o->select = -1;
    o->window_w = 0, o->window_h = 0;
    for (int i = 1; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--game") == 0 && has_value) o->game = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && has_value) o->user = argv[++i];
        else if (strcmp(argv[i], "--psd-root") == 0 && has_value) o->psd_root = argv[++i];
        else if (strcmp(argv[i], "--shot") == 0 && has_value) o->shot = argv[++i];
        else if (strcmp(argv[i], "--window") == 0 && has_value) {
            char tail = '\0';
            if (sscanf(argv[++i], "%dx%d%c", &o->window_w, &o->window_h, &tail) != 2 || o->window_w < WINDOW_MIN_W || o->window_w > WINDOW_MAX_W ||
                o->window_h < WINDOW_MIN_H || o->window_h > WINDOW_MAX_H) {
                fprintf(stderr, "--window wants WxH, %d..%d by %d..%d: %s\n", WINDOW_MIN_W, WINDOW_MAX_W, WINDOW_MIN_H, WINDOW_MAX_H, argv[i]);
                return 0;
            }
        }
        else if (strcmp(argv[i], "--frames") == 0 && has_value) o->frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--select") == 0 && has_value) o->select = atoi(argv[++i]);
        else if (strcmp(argv[i], "--ask") == 0 && has_value) {
            const char *what = argv[++i];
            o->ask = strcmp(what, "quit") == 0 ? APP_ACT_QUIT : strcmp(what, "new") == 0 ? APP_ACT_NEW : strcmp(what, "open") == 0 ? APP_ACT_OPEN
                   : strcmp(what, "opencar") == 0 ? APP_ACT_OPEN_CAR : strcmp(what, "light") == 0 ? APP_ACT_OPEN_LIGHT : strcmp(what, "export") == 0 ? APP_ACT_EXPORT_PICK : APP_ACT_NONE;
            if (o->ask == APP_ACT_NONE) return usage();
        } else if (strcmp(argv[i], "--export-car") == 0 && has_value) o->export_car = argv[++i];
        else if (strcmp(argv[i], "--into") == 0 && has_value) o->export_into = argv[++i];
        else if (strcmp(argv[i], "--pick") == 0 && has_value) o->pick = argv[++i];
        else if (strcmp(argv[i], "--convert-psd") == 0) o->convert_psd = 1;
        else if (strcmp(argv[i], "--race-photo") == 0) o->race_photo = 1;
        else if (strcmp(argv[i], "--photo-view") == 0) o->photo_view = 1;
        else if (strcmp(argv[i], "--night") == 0) o->night = 1;
        else if (strcmp(argv[i], "--orbit") == 0 && has_value) o->orbit = argv[++i];
        else if (strcmp(argv[i], "--add-photo") == 0 && has_value && o->add_photo_count < 8) o->add_photo[o->add_photo_count++] = argv[++i];
        else if (strcmp(argv[i], "--card-backdrop") == 0 && has_value) o->card_backdrop = argv[++i];
        else if (strcmp(argv[i], "--shine") == 0 && has_value) o->shine = argv[++i];
        else if (strcmp(argv[i], "--wheels") == 0 && has_value) o->wheels = argv[++i];
        else if (strcmp(argv[i], "--track") == 0 && has_value) o->track = argv[++i];
        else if (strcmp(argv[i], "--race-png") == 0 && has_value) o->race_png = argv[++i];
        else if (strcmp(argv[i], "--race-off") == 0 && has_value) o->race_off = argv[++i];
        else if (strcmp(argv[i], "--race-scale") == 0 && has_value) {
            const char *v = argv[++i];
            if (strcmp(v, "1") != 0 && strcmp(v, "2") != 0) return usage();   /* 1 (640x480) or 2 (1280x960), nothing else */
            o->race_scale = v[0] - '0';
        }
        else if (strcmp(argv[i], "--light-dir") == 0 && has_value) o->light_dir = argv[++i];
        else if (strcmp(argv[i], "--light") == 0 && has_value) o->light_pick = argv[++i];
        else if (strcmp(argv[i], "--race-view") == 0 && has_value) {
            const char *v = argv[++i];
            if (strcmp(v, "follow") != 0 && strcmp(v, "cockpit") != 0) return usage();
            o->race_cockpit = strcmp(v, "cockpit") == 0;
        }
        else if (strcmp(argv[i], "--gauge-light") == 0 && has_value) {
            const char *v = argv[++i];
            if (v[0] < '0' || v[0] > '5' || v[1] != '\0') return usage();   /* 0..5: the game's gauge_light */
            o->gauge_light = v[0] - '0' + 1;
        }
        else if (strcmp(argv[i], "--compare") == 0 && has_value) {
            o->light_compare = argv[++i];
            if (strcmp(o->light_compare, "off") != 0 && strcmp(o->light_compare, "before") != 0 && strcmp(o->light_compare, "four") != 0) return usage();
        }
        else if (strcmp(argv[i], "--cd") == 0 && has_value) o->cd = argv[++i];
        else if (strcmp(argv[i], "--port-data") == 0 && has_value) o->port_data = argv[++i];
        else if (strcmp(argv[i], "--workspace") == 0 && has_value) o->workspace = argv[++i];
        else if (strcmp(argv[i], "--set") == 0 && has_value && o->set_count < 8) o->set[o->set_count++] = argv[++i];
        else if (strcmp(argv[i], "--changes") == 0) o->changes = 1;
        else if (strcmp(argv[i], "--paint") == 0 && has_value) o->preview_paint = atoi(argv[++i]);
        else if (strcmp(argv[i], "--view") == 0 && has_value) o->view = argv[++i];
        else if (strcmp(argv[i], "--preview-tex") == 0 && has_value) o->preview_tex = argv[++i];
        else if (strcmp(argv[i], "--lights") == 0 && has_value) o->lights = argv[++i];
        else if (strcmp(argv[i], "--night-light") == 0 && has_value) o->night_light = atoi(argv[++i]);
        else if (strcmp(argv[i], "--night-tint") == 0 && has_value) o->night_tint = argv[++i];
        else if (strcmp(argv[i], "--mask") == 0 && has_value) o->mask = argv[++i];
        else if (strcmp(argv[i], "--reset-layout") == 0) o->reset_layout = 1;
        else if (strcmp(argv[i], "--edit") == 0) o->edit = 1;
        else if (strcmp(argv[i], "--steps") == 0 && has_value) o->steps = argv[++i];
        else if (strcmp(argv[i], "--save") == 0) o->save = 1;
        else if (strcmp(argv[i], "--build") == 0) o->build = 1;
        else if (strcmp(argv[i], "--install") == 0) o->install = 1;
        else if (strcmp(argv[i], "--test-drive") == 0) o->test_drive = 1;
        else if (argv[i][0] != '-' && o->dir == NULL) o->dir = argv[i];
        else return usage();
    }
    RaceLook check = car_race_look_default();
    if ((o->race_off != NULL || o->race_scale != 0) && !o->race_photo && !o->test_drive) return usage();   /* the Race look is a Race photo's or a test drive's */
    if ((o->race_cockpit || o->gauge_light != 0) && !o->race_photo) return usage();   /* the dash view is a photo's: a test drive has its own camera */
    if (o->race_off != NULL && !race_off_apply(o->race_off, &check)) return usage();
    if (o->wheels != NULL && strcmp(o->wheels, "still") != 0 && strcmp(o->wheels, "slow") != 0 && strcmp(o->wheels, "fast") != 0) return usage();   /* --wheels still|slow|fast */
    return 1;
}

/* the finder's questions to the file system and the system */
static int sdl_is_file(const char *path, void *ctx)
{
    (void)ctx;
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

#if defined(__APPLE__)
/* the game's app wherever Launch Services knows it by its bundle identifier (renamed, moved, on another volume) */
static int mdfind_game(char *app, size_t cap, void *ctx)
{
    (void)ctx;
    FILE *p = popen("/usr/bin/mdfind \"kMDItemCFBundleIdentifier == '" GAME_FIND_BUNDLE_ID "'\" 2>/dev/null", "r");
    if (p == NULL) return 0;
    const int got = fgets(app, (int)cap, p) != NULL;
    pclose(p);
    if (!got) return 0;
    app[strcspn(app, "\r\n")] = '\0';
    return app[0] != '\0';
}
#endif

/* The game's program, when --game is not given: the one the user located, the app beside this one (the DMG's "Spirited Drive.app" next to
 * "Spirited Drive Studio.app", in the Applications folder or anywhere both were dragged), /Applications, ~/Applications, the app the system knows
 * by its bundle id, else ./tnfs as a build tree has it (game_find.h). `tried` gets every place; `memo` is the file the located game is kept in. */
static int default_game(char *out, size_t cap, char *tried, size_t tried_cap, char *memo, size_t memo_cap)
{
    const char *docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    memo[0] = '\0';
    char remembered[GAME_FIND_PATH] = "";
    if (docs != NULL && snprintf(memo, memo_cap, "%sSpirited Drive Studio/game.txt", docs) < (int)memo_cap) {
        FILE *f = fopen(memo, "rb");
        if (f != NULL) {
            if (fgets(remembered, sizeof remembered, f) != NULL) remembered[strcspn(remembered, "\r\n")] = '\0';
            fclose(f);
        }
    }
    GameFinder finder = { SDL_GetBasePath(), SDL_GetUserFolder(SDL_FOLDER_HOME), remembered, sdl_is_file, NULL, NULL };
#if defined(__APPLE__)
    finder.by_bundle_id = mdfind_game;
#endif
    return game_find(&finder, out, cap, tried, tried_cap);
}

/* the window's own events: closing it, or Escape (which no panel keeps) asks to quit, once no control point is selected; with unsaved changes the answer is asked for first (app_ask),
 * and Escape on that question is Cancel */
static void window_event(App *a, const SDL_Event *ev)
{
    if (ev->type == SDL_EVENT_QUIT) {
        app_ask(a, APP_ACT_QUIT, "");
    } else if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.key == SDLK_ESCAPE && !ev->key.repeat) {
        if (a->confirm != APP_ACT_NONE) app_answer(a, APP_ANSWER_CANCEL);
        else if (ui_popup_open()) return;   /* with a menu open Escape closes the menu, nothing more */
        else if (a->kind == APP_KIND_TRACK && app_sel_count(a) > 0) app_sel_clear(a);   /* several points (or one) selected: Escape lets them go first */
        else app_ask(a, APP_ACT_QUIT, "");
    }
}

/* The native folder dialog (SDL3's: the system's own) is asynchronous and its callback may come from another thread: it leaves the folder here,
 * and the main loop takes it. */
typedef struct {
    SDL_Mutex *lock;
    int open;                /* a dialog is on screen */
    int ready;               /* it was answered: `path` is the folder, "" when it was cancelled */
    AppAction kind;          /* APP_ACT_NEW, APP_ACT_OPEN, APP_ACT_OPEN_CAR, APP_ACT_EXPORT_PICK, APP_ACT_LIGHT_PICK or APP_ACT_WORKSPACE_PICK (folders); APP_ACT_BACKDROP_PICK (a PNG file) */
    char path[APP_PATH];
} Dialog;

static Dialog g_dialog;

static void SDLCALL dialog_done(void *userdata, const char *const *list, int filter)
{
    Dialog *d = userdata;
    (void)filter;
    SDL_LockMutex(d->lock);
    snprintf(d->path, sizeof d->path, "%s", list != NULL && list[0] != NULL ? list[0] : "");
    d->ready = 1;
    d->open = 0;
    SDL_UnlockMutex(d->lock);
}

/* Shows the dialog for what `kind` asks, starting where the project that is open lives (its folder's parent). */
static void dialog_start(SDL_Window *win, App *a, AppAction kind)
{
    if (g_dialog.lock == NULL) g_dialog.lock = SDL_CreateMutex();
    if (g_dialog.lock == NULL || g_dialog.open) return;
    char where[APP_PATH];
    if (kind == APP_ACT_LIGHT_PICK) {   /* beside the light folder's own parent */
        snprintf(where, sizeof where, "%s", a->light_dir);
        char *slash = strrchr(where, '/');
        if (slash != NULL && slash != where) *slash = '\0';
    } else app_dialog_dir(a, kind, where, sizeof where);   /* the workspace's cars/ or tracks/, else the open project's parent, else "": the system's own place */
    g_dialog.open = 1;
    g_dialog.ready = 0;
    g_dialog.kind = kind;
    SDL_ShowOpenFolderDialog(dialog_done, &g_dialog, win, where[0] != '\0' ? where : NULL, false);
}

/* The file dialog for a PNG picture (the card photo's backdrop, or a showcase photo: `kind`), starting beside the picture already chosen, else where the open car lives */
static void file_dialog_start(SDL_Window *win, const App *a, const char *from, AppAction kind)
{
    if (g_dialog.lock == NULL) g_dialog.lock = SDL_CreateMutex();
    if (g_dialog.lock == NULL || g_dialog.open) return;
    static const SDL_DialogFileFilter PNG_FILES[] = { { "PNG pictures", "png" } };
    char where[APP_PATH];
    snprintf(where, sizeof where, "%s", from[0] != '\0' ? from : a->dir);
    char *slash = strrchr(where, '/');
    if (from[0] != '\0' && slash != NULL && slash != where) *slash = '\0';
    g_dialog.open = 1;
    g_dialog.ready = 0;
    g_dialog.kind = kind;
    SDL_ShowOpenFileDialog(dialog_done, &g_dialog, win, PNG_FILES, 1, where[0] != '\0' ? where : NULL, false);
}

/* what to do with an action that is ready to go (confirmed, or nothing was unsaved); 1 to quit */
static int run_go(SDL_Window *win, App *a)
{
    const AppAction act = a->go;
    char path[APP_PATH];
    snprintf(path, sizeof path, "%s", a->go_path);
    a->go = APP_ACT_NONE;
    if (act == APP_ACT_QUIT) return 1;
    if (act == APP_ACT_NEW && a->workspace[0] != '\0') a->new_track_dialog = 1, a->new_track_name[0] = a->new_track_id[0] = a->new_track_error[0] = '\0', a->new_track_id_typed = 0;   /* a workspace: only a name is asked for */
    else if (act == APP_ACT_NEW || act == APP_ACT_OPEN || act == APP_ACT_OPEN_CAR || act == APP_ACT_EXPORT_PICK || act == APP_ACT_LIGHT_PICK || act == APP_ACT_WORKSPACE_PICK) dialog_start(win, a, act);
    else if (act == APP_ACT_EXPORT_AT) app_export_car(a, path);
    else if (act == APP_ACT_RECENT) app_open_recent(a, path);
    else if (act == APP_ACT_OPEN_AT) app_open_project(a, path);
    else if (act == APP_ACT_OPEN_CAR_AT) app_open_car(a, path);
    else if (act == APP_ACT_OPEN_LIGHT) app_open_light(a);
    else if (act == APP_ACT_LIGHT_AT) app_light_revert(a), app_light_set_folder(a, path);   /* asked about unsaved changes first: what is left is to be dropped */
    else if (act == APP_ACT_NEW_AT) app_new_project(a, path);
    else if (act == APP_ACT_MAIN) app_close_project(a);
    return 0;
}

/* the folder the dialog gave, when it did: the project is opened (or made) in it; a cancelled dialog changes nothing */
/* a->reveal in the Finder: a folder opened, a file shown selected in its folder (`open -R`, no shell; elsewhere its folder opened) */
static void reveal(App *a)
{
    char path[APP_PATH], url[APP_PATH * 3];
    snprintf(path, sizeof path, "%s", a->reveal);
    struct stat st;
    const int file = stat(path, &st) == 0 && S_ISREG(st.st_mode);
#ifdef __APPLE__
    if (file) {
        char *argv[] = {(char *)"open", (char *)"-R", path, NULL};
        pid_t pid;
        int status = 0;
        const int rc = posix_spawnp(&pid, "open", NULL, NULL, argv, environ);
        if (rc != 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            snprintf(a->status, sizeof a->status, "cannot show %s in the Finder", path);
        return;
    }
#endif
    if (file) {
        char *slash = strrchr(path, '/');
        if (slash != NULL) *slash = '\0';
    }
    if (car_export_file_url(path, url, sizeof url) && !SDL_OpenURL(url)) snprintf(a->status, sizeof a->status, "cannot open %s: %s", path, SDL_GetError());
}

static void take_dialog(App *a)
{
    if (g_dialog.lock == NULL) return;
    char path[APP_PATH] = "";
    AppAction kind = APP_ACT_NONE;
    int answered = 0;
    SDL_LockMutex(g_dialog.lock);
    if (g_dialog.ready) {
        answered = 1;
        g_dialog.ready = 0;
        kind = g_dialog.kind;
        snprintf(path, sizeof path, "%s", g_dialog.path);
    }
    SDL_UnlockMutex(g_dialog.lock);
    if (path[0] == '\0') {
        if (answered) app_pick_cancelled(a);   /* a cancelled picker: the answer given before it is dropped */
        return;
    }
    if (kind == APP_ACT_PHOTO_PICK) {   /* a showcase photo: copied into the car's folder, named on the card (an undo step); no question */
        app_add_photo(a, path);
        return;
    }
    if (kind == APP_ACT_BACKDROP_PICK) {   /* a picture for the card photo: nothing in the project changes, no question */
        ui_card_backdrop_file(path);
        return;
    }
    if (kind == APP_ACT_WORKSPACE_PICK) {   /* it changes no project: no question about unsaved changes */
        app_workspace_set(a, path);
        return;
    }
    app_ask(a, kind == APP_ACT_NEW ? APP_ACT_NEW_AT : kind == APP_ACT_OPEN_CAR ? APP_ACT_OPEN_CAR_AT : kind == APP_ACT_EXPORT_PICK ? APP_ACT_EXPORT_AT
                : kind == APP_ACT_LIGHT_PICK ? APP_ACT_LIGHT_AT : APP_ACT_OPEN_AT, path);   /* through the same question: what is on screen may have changed while the dialog was up */
}

/* the car of Export original car whose masks folder is `folder` (any case) chosen; 0 when there is none */
static int pick_car(App *a, const char *folder)
{
    for (int i = 0; car_export_car(i) != NULL; ++i)
        if (strcasecmp(car_export_car(i)->folder, folder) == 0) return a->export_pick = i, 1;
    return 0;
}

/* the window's title as the app has it (the project and its unsaved mark), set when it changes */
static void set_title(SDL_Window *win, const App *a)
{
    char title[APP_PATH + 64];
    app_title(a, title, sizeof title);
    if (strcmp(title, SDL_GetWindowTitle(win)) != 0) SDL_SetWindowTitle(win, title);
}

/* --install: the window's Install, waited for; the installer's own output ("installed <id> <version>") goes to stdout as the game prints it. 1 when it installed. */
static int install_waited(App *a)
{
    int made = app_install(a);
    while (made && app_install_poll(a)) SDL_Delay(20);
    if (made && a->install.run.log != NULL) fputs(a->install.run.log, stdout);
    return made && a->install.ok;
}

/* --test-drive: the Drive window's button, waited for until the game's log has the track's line (printed), then the game is stopped. 1 when it took the track. */
static int test_drive(App *a, const Options *o)
{
    if (o->race_off != NULL) (void)race_off_apply(o->race_off, &a->race_look);   /* checked whole in parse_args */
    if (o->race_scale != 0) a->race_look.scale = o->race_scale;
    int made = app_test_drive(a);
    for (int i = 0; made && car_race_installing(&a->race) && i < TEST_DRIVE_WAIT_MS / 20 && app_race_poll(a); ++i) SDL_Delay(20);   /* the install first, */
    for (int i = 0; made && i < TEST_DRIVE_WAIT_MS / 20 && a->race.piece_track == 0 && app_race_poll(a); ++i) SDL_Delay(20);   /* then the game's line */
    if (made) {
        if (a->race.piece_line[0] != '\0') fprintf(stdout, "%s\n", a->race.piece_line);
        app_race_stop(a);
        while (app_race_poll(a)) SDL_Delay(20);
        made = a->race.piece_track > 0;
    }
    fprintf(made ? stdout : stderr, "%s\n", a->status);
    return made;
}

/* the actions asked for on the command line, once, before the first frame; 1 when all of them went */
static int run_requested(App *a, const Options *o)
{
    int ok = 1;
    if (o->frames > 0) while (app_car_build_poll(a)) SDL_Delay(5);   /* a headless run waits for the kit to be built: its pictures show the car */
    if (o->save) ok &= app_save(a);
    if (o->build) {
        ok &= app_build(a);
        if (ok && app_car_building(a)) {   /* a car kit builds in a thread: wait for it, then say how it ended */
            while (app_car_build_poll(a)) SDL_Delay(5);
            ok &= a->car_build.ok;
        }
    }
    if (o->install) ok &= install_waited(a);
    if (o->save || o->build || o->install) fprintf(stdout, "%s\n", a->status);
    if (o->test_drive) ok &= test_drive(a, o);
    if (o->export_car != NULL) {   /* Export original car, headless: the project is made in --into and opened */
        const int made = pick_car(a, o->export_car) && o->export_into != NULL && app_export_car(a, o->export_into);
        if (!made && a->status[0] == '\0') snprintf(a->status, sizeof a->status, "--export-car %s needs a car of the list and --into PARENT", o->export_car);
        fprintf(made ? stdout : stderr, "%s\n", a->status);
        ok &= made;
        if (made && o->frames > 0) while (app_car_build_poll(a)) SDL_Delay(5);   /* the exported kit is built as it opens */
    }
    if (o->changes) {   /* the masks against the game's, headless */
        const ExportCar *car = car_race_original_of(&a->car);
        char project[APP_PATH], game[APP_PATH], why[256], text[4096];
        MaskChanges *changes = malloc(sizeof *changes);
        int done = a->kind == APP_KIND_CAR && car != NULL && changes != NULL;
        if (done) {
            car_project_masks_dir(&a->car, project, sizeof project);
            snprintf(game, sizeof game, "%s/car_masks/%s", a->export_src.data, car->folder);
            done = mask_changes_compare(project, game, changes, why, sizeof why);
        } else {
            snprintf(why, sizeof why, "--changes needs an open car of the game");
        }
        if (done) mask_changes_text(changes, car->folder, text, sizeof text);
        fprintf(done ? stdout : stderr, "%s", done ? text : why);
        if (!done) fputc('\n', stderr);
        free(changes);
        ok &= done;
    }
    if (o->preview_paint > 0) a->preview_paint = o->preview_paint - 1;
    if (o->view != NULL) {
        if (strcmp(o->view, "night") == 0) a->night.night = 1;
        else if (strcmp(o->view, "day") != 0) fprintf(stderr, "--view %s: day or night\n", o->view);
    }
    if (o->lights != NULL) {   /* the lamps named are on, the others off */
        a->night.lights = a->night.brake = a->night.reverse = a->night.police = 0;
        char list[160];
        snprintf(list, sizeof list, "%s", o->lights);
        for (char *w = strtok(list, ","); w != NULL; w = strtok(NULL, ",")) {
            if (strcmp(w, "head") == 0 || strcmp(w, "side") == 0) a->night.lights = 1;
            else if (strcmp(w, "brake") == 0) a->night.brake = 1;
            else if (strcmp(w, "reverse") == 0) a->night.reverse = 1;
            else if (strcmp(w, "police") == 0) a->night.police = 1;
            else if (strcmp(w, "none") != 0) fprintf(stderr, "--lights %s: head, side, brake, reverse, police or none\n", w);
        }
    }
    if (o->preview_tex != NULL) snprintf(a->preview_focus, sizeof a->preview_focus, "%.4s", o->preview_tex);
    if (o->night_light > 0) a->night.light = o->night_light < NIGHT_LIGHT_MIN ? NIGHT_LIGHT_MIN : o->night_light > NIGHT_LIGHT_MAX ? NIGHT_LIGHT_MAX : o->night_light;
    if (o->night_tint != NULL) {
        float t[3];
        if (sscanf(o->night_tint, "%f,%f,%f", &t[0], &t[1], &t[2]) != 3 || t[0] < 0 || t[1] < 0 || t[2] < 0 || t[0] > 1 || t[1] > 1 || t[2] > 1)
            fprintf(stderr, "--night-tint %s: R,G,B, each 0 to 1 expected\n", o->night_tint);
        else
            for (int i = 0; i < 3; ++i) a->night.tint[i] = t[i];
    }
    if (o->mask != NULL) {   /* the mask view, headless: the mask is opened */
        int done = app_mask_open_by_name(a, o->mask);
        if (!done && a->mask.error[0] != '\0') snprintf(a->status, sizeof a->status, "%s", a->mask.error);
        fprintf(done ? stdout : stderr, "%s\n", a->status);
        ok &= done;
    }
    if (o->convert_psd) {   /* Convert PSD as the Masks button runs it, waited for */
        while (a->kind == APP_KIND_CAR && a->py.state != 2) {   /* the python with psd-tools is looked for first */
            app_psd_poll(a);
            SDL_Delay(20);
        }
        int done = app_psd_start(a);
        while (done && app_psd_poll(a)) SDL_Delay(20);
        done = done && a->psd.ok;
        fprintf(done ? stdout : stderr, "%s\n", a->status);
        ok &= done;
    }
    if (o->race_photo) {   /* Race photo, headless: the game is run and waited for, the picture is in the Race window */
        if (o->track != NULL && car_race_track_find(o->track) >= 0) a->race_track = car_race_track_find(o->track);
        a->race_night = o->night;
        if (o->race_off != NULL) (void)race_off_apply(o->race_off, &a->race_look);   /* checked whole in parse_args */
        if (o->race_scale != 0) a->race_look.scale = o->race_scale;
        a->race_look.cockpit = o->race_cockpit;
        if (o->gauge_light != 0) a->race_look.gauge_set = 1, a->race_look.gauge_light = o->gauge_light - 1;
        int made = app_race(a, 1);
        while (made && app_race_poll(a)) SDL_Delay(20);
        a->photo_view_open = o->photo_view;
        made = made && a->race.ok;
        if (made && o->race_png != NULL) {
            uint8_t *png = NULL;
            size_t len = 0;
            FILE *f = NULL;
            made = png_write_rgba(a->race.photo, (uint32_t)a->race.photo_w, (uint32_t)a->race.photo_h, &png, &len) == PNG_OK && (f = fopen(o->race_png, "wb")) != NULL &&
                   fwrite(png, 1, len, f) == len;
            if (f != NULL) fclose(f);
            free(png);
            if (!made) snprintf(a->status, sizeof a->status, "cannot write %s", o->race_png);
        }
        fprintf(made ? stdout : stderr, "%s\n", a->status);
        ok &= made;
    }
    return ok;
}

static int run(App *a, const Options *o)
{
    if (o->shot != NULL) SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    int window_w = o->window_w, window_h = o->window_h;
    if (window_w == 0) {   /* the default: a headless run keeps its pictures' size, a normal run takes what the main display leaves (menu bar and Dock out) */
        SDL_Rect usable = { 0, 0, 0, 0 };
        if (o->frames > 0 || o->shot != NULL || !SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)) window_w = WINDOW_W, window_h = WINDOW_H;
        else window_fit(usable.w, usable.h, &window_w, &window_h);
    }
    SDL_Window *win = SDL_CreateWindow("Spirited Drive Studio", window_w, window_h, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = win != NULL ? SDL_CreateRenderer(win, NULL) : NULL;
    if (ren == NULL) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        if (win != NULL) SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    char ui_why[APP_MSG];
    char ini[APP_PATH] = "";
    if (o->user == NULL && o->frames == 0) {   /* a normal run without --user keeps its layout in the editor's own folder */
        const char *pref = SDL_GetPrefPath(NULL, "spirited-drive-studio");
        if (pref != NULL) snprintf(ini, sizeof ini, "%strack-editor.ini", pref);
    } else if (o->user != NULL) {   /* the window's layout is kept in the player folder, made when it is not there yet */
        const int n = snprintf(ini, sizeof ini, "%s/track-editor.ini", o->user);
        if (n < 0 || (size_t)n >= sizeof ini || !SDL_CreateDirectory(o->user)) {
            fprintf(stderr, "cannot keep the layout in %s: %s\n", o->user, SDL_GetError());
            ini[0] = '\0';
        }
    }
    if (!ui_open(win, ren, ini[0] != '\0' ? ini : NULL, ui_why, sizeof ui_why)) {
        fprintf(stderr, "%s\n", ui_why);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    if (o->steps != NULL) ui_double_click_by_frames();
    ui_window_default(window_w, window_h);
    if (o->reset_layout) ui_reset_layout();
    if (o->card_backdrop != NULL) ui_card_backdrop_file(o->card_backdrop);
    if (o->orbit != NULL) {
        double yaw = 0, pitch = 0;
        int dist = 0;
        if (sscanf(o->orbit, "%lf,%lf,%d", &yaw, &pitch, &dist) < 2) {
            fprintf(stderr, "--orbit %s: YAW,PITCH[,DIST] expected\n", o->orbit);
            ui_close();
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 0;   /* a run that did not go */
        }
        ui_model_orbit(yaw, pitch, dist);
    }
    if (o->shine != NULL) {
        if (strcmp(o->shine, "on") != 0 && strcmp(o->shine, "off") != 0) {
            fprintf(stderr, "--shine %s: on or off expected\n", o->shine);
            ui_close();
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 0;   /* a run that did not go */
        }
        ui_model_shine(strcmp(o->shine, "on") == 0);
    }
    if (o->wheels != NULL) {   /* the Model's Wheels, as chosen in its window */
        a->model_wheels = strcmp(o->wheels, "fast") == 0 ? 2 : strcmp(o->wheels, "slow") == 0 ? 1 : 0;
        a->model_wheels_fixed = o->frames > 0;   /* Slow: its first frame, the same picture every run */
    }
    if (a->kind == APP_KIND_TRACK && o->select >= 0 && o->select < a->trace.count) a->selected = o->select;   /* for a headless picture of the Node window */
    for (int i = 0; i < o->set_count; ++i) {   /* a car's fields, as typed */
        if (a->kind != APP_KIND_CAR || !car_edit_set_by_text(&a->edit, o->set[i])) {
            fprintf(stderr, "--set %s: not a field of the open car's card\n", o->set[i]);
            ui_close();
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 0;   /* a run that did not go */
        }
    }
    for (int i = 0; i < o->add_photo_count; ++i) {   /* showcase photos, as Add photo... does it */
        if (!app_add_photo(a, o->add_photo[i])) {
            fprintf(stderr, "--add-photo %s: %s\n", o->add_photo[i], a->status);
            ui_close();
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 0;   /* a run that did not go */
        }
    }
    if (o->edit) app_add_point(a);   /* and of the confirmation: something unsaved ... */
    if (o->pick != NULL && !pick_car(a, o->pick)) {
        fprintf(stderr, "--pick %s: not a car Export original car knows\n", o->pick);
        ui_close();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 0;
    }
    if (o->ask == APP_ACT_OPEN_LIGHT) {   /* the window itself, for a headless picture of it (a headless run drops a->go) */
        if (app_open_light(a) && o->light_pick != NULL) {
            char track[8] = "", time = '\0';
            long node = 0;
            int t = -1;
            if (sscanf(o->light_pick, "%7[A-Z0-9]:%c:%ld", track, &time, &node) == 3)
                for (int i = 0; i < LIGHT_UI_TIMES; ++i)
                    if (light_ui_time_letter(i) == time) t = i;
            int track_index = -1;
            for (int i = 0; light_ui_track(i) != NULL; ++i)
                if (strcmp(light_ui_track(i), track) == 0) track_index = i;
            if (t < 0 || track_index < 0 || !app_light_select(a, track_index, t)) fprintf(stderr, "--light %s: TRACK:TIME:NODE such as CY1:N:70, the track one that has a light file\n", o->light_pick);
            else app_light_set_node(a, node);
        }
        if (a->kind == APP_KIND_LIGHT && o->light_compare != NULL)
            app_light_set_compare(a, strcmp(o->light_compare, "four") == 0 ? LIGHT_COMPARE_FOUR : strcmp(o->light_compare, "before") == 0 ? LIGHT_COMPARE_BEFORE : LIGHT_COMPARE_OFF);
    }
    else if (o->ask == APP_ACT_EXPORT_PICK) a->export_dialog = 1;   /* the window itself, for a headless picture of it */
    else if (o->ask != APP_ACT_NONE) app_ask(a, o->ask, "");   /* ... and the action asked for */
    if (o->user == NULL && o->frames == 0) {   /* a normal run keeps the recent projects in the editor's own folder */
        const char *pref = SDL_GetPrefPath(NULL, "spirited-drive-studio");
        char file[APP_PATH];
        if (pref != NULL && snprintf(file, sizeof file, "%srecent.txt", pref) < (int)sizeof file) app_keep_recent(a, file);
        if (pref != NULL && snprintf(file, sizeof file, "%sworkspace.txt", pref) < (int)sizeof file) app_workspace_keep(a, file);
        if (a->kind == APP_KIND_NONE && a->recent.count > 0 && recent_present(a->recent.dir[0], a->recent.kind[0])) {   /* nothing given: the project it had last */
            char last[APP_PATH];
            snprintf(last, sizeof last, "%s", a->recent.dir[0]);
            app_open_recent(a, last);
            a->open_error[0] = '\0';   /* a restore that fails is the start screen, not a message nobody asked for */
        }
    }
    if (o->workspace != NULL && !app_workspace_set(a, o->workspace)) fprintf(stderr, "--workspace %s: %s\n", o->workspace, a->status);
    const int headless = o->frames > 0;
    ui_settings_to(a);
    int ok = run_requested(a, o), frame = 0, quit = 0;
    static Steps steps;   /* --steps: after what the command line asked for, on what it opened (the mask --mask opened, the card --set changed) */
    if (o->steps != NULL && !steps_read(&steps, o->steps)) ok = 0, steps.failed = 1;
    while (!quit) {
        for (SDL_Event ev; SDL_PollEvent(&ev);) {
            ui_event(&ev);
            window_event(a, &ev);
        }
        take_dialog(a);
        if (a->backdrop_dialog) {   /* the Model window's Choose... (a headless run has the typed path and --card-backdrop) */
            a->backdrop_dialog = 0;
            if (!headless) file_dialog_start(win, a, ui_card_backdrop_path(), APP_ACT_BACKDROP_PICK);
        }
        if (a->photo_dialog) {   /* the Car window's Add photo... (a headless run has --add-photo) */
            a->photo_dialog = 0;
            if (!headless) file_dialog_start(win, a, "", APP_ACT_PHOTO_PICK);
        }
        if (a->reveal[0] != '\0') {   /* a folder or file to show in the Finder (the Masks window's and the Mask panel's buttons) */
            if (!headless) reveal(a);
            a->reveal[0] = '\0';
        }
        if (a->go != APP_ACT_NONE) {
            if (headless) a->go = APP_ACT_NONE;   /* a headless run ends by its frames: no dialog, no quit of its own */
            else if (run_go(win, a)) quit = 1;
        }
        app_car_build_poll(a);
        app_install_poll(a);
        app_psd_poll(a);
        app_dash_poll(a);
        app_race_poll(a);
        app_card_watch(a, SDL_GetTicks() / 1000.0);   /* mod.ini, the car file and the textures */
        const int light_busy = app_light_poll(a, SDL_GetTicks() / 1000.0);   /* the Track light photo: its output, and the next one when a change has settled */
        app_mask_watch(a, SDL_GetTicks() / 1000.0);   /* masks changed on disk by another program: about once a second */
        app_sheet_poll(a);
        app_recompute(a);
        SDL_SetRenderDrawColor(ren, 18, 20, 24, 255);
        SDL_RenderClear(ren);
        ui_frame(ren, a);
        if (o->steps != NULL) steps_frame(&steps, a, win);
        set_title(win, a);
        ++frame;
        /* a headless run waits for its steps, and for the Track light pictures in the making (bounded: a game that never ends cannot hold it) */
        if (headless && frame >= o->frames && !steps_running(&steps) && !(light_busy && frame < o->frames + LIGHT_WAIT_FRAMES)) {
            if (o->shot != NULL) {
                char why[APP_MSG];
                if (!capture_save_png(ren, o->shot, why, sizeof why)) {
                    fprintf(stderr, "%s\n", why);
                    ok = 0;
                } else {
                    fprintf(stdout, "wrote %s\n", o->shot);
                }
            }
            SDL_RenderPresent(ren);
            break;
        }
        SDL_RenderPresent(ren);
        SDL_Delay(FRAME_MS);
    }
    ui_close();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return ok && !steps.failed;
}

int main(int argc, char **argv)
{
    Options o;
    if (!parse_args(argc, argv, &o)) return 2;
    char game[APP_PATH], tried[GAME_FIND_TRIED] = "", memo[APP_PATH] = "";
    if (o.game == NULL) {
        default_game(game, sizeof game, tried, sizeof tried, memo, sizeof memo);
        o.game = game;
    }
    App *app = calloc(1, sizeof *app);
    if (app == NULL) return 1;
    int status = 1;
    char generic[APP_PATH];
    const char *generic_art = art_generic_find(generic, sizeof generic) ? generic : NULL;
    int opened = 1;
    if (o.dir != NULL) opened = app_open_any(app, o.dir, o.game, o.user != NULL ? o.user : "", generic_art);
    else app_begin(app, o.game, o.user != NULL ? o.user : "", generic_art);   /* nothing given: the start screen, or what it had last */
    app_psd_setup(app, SDL_GetBasePath(), o.psd_root);
    if (opened) {
        ExportSource sources;
        char *pref = o.cd == NULL && o.user == NULL ? SDL_GetPrefPath(NULL, "Spirited Drive") : NULL;   /* the game's player folder, where its own copy of the game's files is: not asked (it makes the folder) when something names it */
        car_export_source(o.game, o.user != NULL ? o.user : "", pref, o.cd, o.port_data, &sources);
        SDL_free(pref);
        app_set_export_source(app, &sources);
        app_set_game_lookup(app, tried, memo);
        app_set_light_dir(app, o.light_dir != NULL ? o.light_dir : STUDIO_LIGHT_DIR);   /* the folder the Track light window tunes: named, or the source tree's data/ it was built from */
        app_check_game(app);   /* the window opens with what to do if the game is not found, not with a failed install later */
        status = run(app, &o) ? 0 : 1;
    } else {
        fprintf(stderr, "%s\n", app->status);
    }
    app_free(app);
    free(app);
    return status;
}
