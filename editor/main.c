/* tnfs-track-editor: Spirited Drive Studio's window. A top view of a track project's centre line (phase 1a, notes/dep59-track-editor-design.md), or a
 * car project's card, textures and masks (dep59 car editor 1d, read only for now); with nothing open, a start screen.
 *
 *   tnfs-track-editor [PROJECT] [--game PATH] [--user DIR]
 *                     [--frames N] [--save] [--build] [--install] [--select N] [--orbit YAW,PITCH[,DIST]] [--edit] [--set KEY=VALUE] [--rehacer] [--ask quit|new|open|opencar] [--shot OUT.png]
 *
 * PROJECT is a folder such as art/tnfs-tracks/<id>/ (created with its project.ini and trace.json when missing), or a car's kit folder such as
 * art/tnfs-cars/<kit>/ (or its coche/): a folder with a project.ini is a track, one with a car's mod.ini is a car. The build uses no CD data:
 * the road is made on the generic template and the stage's art is the FAM named by project.ini's art=. --game and --user are for the
 * install (the game's --install; without --user, into the game's own player folder). Without PROJECT the editor reopens the project it had last, or shows
 * the start screen; without --game it takes the Spirited Drive app beside it (inside the DMG) or ./tnfs. The options after --frames run headless: N frames, then they are done, and --shot writes the last frame to a PNG; the
 * exit status is 1 when a --save, --build or --install failed. Run it with SDL_VIDEO_DRIVER=dummy and SDL_AUDIO_DRIVER=dummy for a headless
 * run. */
#include "app.h"
#include "art.h"
#include "build.h"
#include "capture.h"
#include "game_find.h"
#include "mask_changes.h"
#include "formats/png.h"
#include "ui.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum {
    WINDOW_W = 960,
    WINDOW_H = 600,
    WINDOW_MIN_W = 640, WINDOW_MIN_H = 400, WINDOW_MAX_W = 3840, WINDOW_MAX_H = 2160,   /* --window's range */
    FRAME_MS = 16
};

typedef struct {
    const char *dir, *game, *user, *shot;
    int frames, save, build, install, select;   /* select: a control point to select before the first frame, -1 for none */
    const char *set[8];      /* --set key=value, a car's field set before the first frame (a headless picture of a problem, a save of a change) */
    int set_count;
    const char *pick;        /* --pick FOLDER: the car chosen in the Export window before the first frame */
    const char *export_car, *export_into, *cd, *port_data;   /* --export-car STEM --into DIR, --cd DIR, --port-data DIR: Export original car without the window's dialogs */
    int race_photo, night;    /* --race-photo: take the race photo before the first frame (a headless run waits for it); --night: the Night preset */
    const char *track, *race_png;   /* --track STEM, --race-png FILE: the track and where the photo is also written */
    const char *orbit;       /* --orbit YAW,PITCH[,DIST]: the Model window's camera, in degrees (a headless picture of a view) */
    const char *mask;        /* --mask NAME: the mask (topv, frnt_lo) opened in Edit mask before the first frame */
    const char *mask_rect[8];   /* --mask-rect X,Y,W,H,CLASS: a rectangle of the open mask set to a class value (one step each), for a headless picture of an edit */
    int mask_rect_count, mask_save;   /* --mask-save: the mask is saved before the first frame */
    const char *view, *lights, *night_track, *preview_tex;   /* --view day|night, --lights head,side,brake,reverse,police, --night-track STEM: the Preview's Day / Night and its lamps, for a headless picture */
    int changes;             /* --changes: the project's masks compared with the game's, the list printed (what Copy the changed list copies) */
    int preview_paint;       /* --paint N: the card's N-th paint (from 1) shown in Preview, for a headless picture; 0 the car as its file has it */
    int rehacer;             /* --rehacer: run the open car's rehacer.command before the first frame (a headless run waits for it) */
    int edit;                /* a point added before the first frame: unsaved changes, for a headless picture of the confirmation */
    AppAction ask;           /* an action asked for before the first frame (quit, new, open) */
    int window_w, window_h;  /* --window WxH: the window's size (a headless picture of a layout at another size); WINDOW_W x WINDOW_H by default */
} Options;

static int usage(void)
{
    fprintf(stderr, "usage: tnfs-track-editor [PROJECT] [--game PATH] [--user DIR] [--frames N] [--save] [--build] "
                    "[--install] [--select N] [--edit] [--set KEY=VALUE] [--rehacer] [--ask quit|new|open|opencar|export] [--shot OUT.png]\n"
                    "       [--export-car FOLDER [--into PARENT]] [--cd DIR] [--port-data DIR] [--pick FOLDER] [--race-photo [--night] [--track STEM] [--race-png OUT.png]]\n"
                    "       [--mask NAME [--mask-rect X,Y,W,H,CLASS]... [--mask-save]] [--paint N] [--view day|night] [--lights head,side,brake,reverse,police] [--night-track STEM] [--preview-tex NAME] [--changes]\n"
                    "       [--window WxH]\n");
    return 0;
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    o->select = -1;
    o->window_w = WINDOW_W, o->window_h = WINDOW_H;
    for (int i = 1; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--game") == 0 && has_value) o->game = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && has_value) o->user = argv[++i];
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
                   : strcmp(what, "opencar") == 0 ? APP_ACT_OPEN_CAR : strcmp(what, "export") == 0 ? APP_ACT_EXPORT_PICK : APP_ACT_NONE;
            if (o->ask == APP_ACT_NONE) return usage();
        } else if (strcmp(argv[i], "--export-car") == 0 && has_value) o->export_car = argv[++i];
        else if (strcmp(argv[i], "--into") == 0 && has_value) o->export_into = argv[++i];
        else if (strcmp(argv[i], "--pick") == 0 && has_value) o->pick = argv[++i];
        else if (strcmp(argv[i], "--race-photo") == 0) o->race_photo = 1;
        else if (strcmp(argv[i], "--night") == 0) o->night = 1;
        else if (strcmp(argv[i], "--orbit") == 0 && has_value) o->orbit = argv[++i];
        else if (strcmp(argv[i], "--track") == 0 && has_value) o->track = argv[++i];
        else if (strcmp(argv[i], "--race-png") == 0 && has_value) o->race_png = argv[++i];
        else if (strcmp(argv[i], "--cd") == 0 && has_value) o->cd = argv[++i];
        else if (strcmp(argv[i], "--port-data") == 0 && has_value) o->port_data = argv[++i];
        else if (strcmp(argv[i], "--set") == 0 && has_value && o->set_count < 8) o->set[o->set_count++] = argv[++i];
        else if (strcmp(argv[i], "--changes") == 0) o->changes = 1;
        else if (strcmp(argv[i], "--paint") == 0 && has_value) o->preview_paint = atoi(argv[++i]);
        else if (strcmp(argv[i], "--view") == 0 && has_value) o->view = argv[++i];
        else if (strcmp(argv[i], "--preview-tex") == 0 && has_value) o->preview_tex = argv[++i];
        else if (strcmp(argv[i], "--lights") == 0 && has_value) o->lights = argv[++i];
        else if (strcmp(argv[i], "--night-track") == 0 && has_value) o->night_track = argv[++i];
        else if (strcmp(argv[i], "--mask") == 0 && has_value) o->mask = argv[++i];
        else if (strcmp(argv[i], "--mask-rect") == 0 && has_value && o->mask_rect_count < 8) o->mask_rect[o->mask_rect_count++] = argv[++i];
        else if (strcmp(argv[i], "--mask-save") == 0) o->mask_save = 1;
        else if (strcmp(argv[i], "--rehacer") == 0) o->rehacer = 1;
        else if (strcmp(argv[i], "--edit") == 0) o->edit = 1;
        else if (strcmp(argv[i], "--save") == 0) o->save = 1;
        else if (strcmp(argv[i], "--build") == 0) o->build = 1;
        else if (strcmp(argv[i], "--install") == 0) o->install = 1;
        else if (argv[i][0] != '-' && o->dir == NULL) o->dir = argv[i];
        else return usage();
    }
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

/* the window's own events: closing it, or Escape (which no panel keeps) asks to quit; with unsaved changes the answer is asked for first (app_ask),
 * and Escape on that question is Cancel */
static void window_event(App *a, const SDL_Event *ev)
{
    if (ev->type == SDL_EVENT_QUIT) {
        app_ask(a, APP_ACT_QUIT, "");
    } else if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.key == SDLK_ESCAPE && !ev->key.repeat) {
        if (a->confirm != APP_ACT_NONE) app_answer(a, APP_ANSWER_CANCEL);
        else if (!ui_popup_open()) app_ask(a, APP_ACT_QUIT, "");   /* with a menu open Escape closes the menu, nothing more */
    }
}

/* The native folder dialog (SDL3's: the system's own) is asynchronous and its callback may come from another thread: it leaves the folder here,
 * and the main loop takes it. */
typedef struct {
    SDL_Mutex *lock;
    int open;                /* a dialog is on screen */
    int ready;               /* it was answered: `path` is the folder, "" when it was cancelled */
    AppAction kind;          /* APP_ACT_NEW, APP_ACT_OPEN, APP_ACT_OPEN_CAR or APP_ACT_EXPORT_PICK */
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
    snprintf(where, sizeof where, "%s", a->dir);   /* "" with nothing open: the system's own place */
    char *slash = strrchr(where, '/');
    if (slash != NULL && slash != where) *slash = '\0';
    g_dialog.open = 1;
    g_dialog.ready = 0;
    g_dialog.kind = kind;
    SDL_ShowOpenFolderDialog(dialog_done, &g_dialog, win, where[0] != '\0' ? where : NULL, false);
}

/* what to do with an action that is ready to go (confirmed, or nothing was unsaved); 1 to quit */
static int run_go(SDL_Window *win, App *a)
{
    const AppAction act = a->go;
    char path[APP_PATH];
    snprintf(path, sizeof path, "%s", a->go_path);
    a->go = APP_ACT_NONE;
    if (act == APP_ACT_QUIT) return 1;
    if (act == APP_ACT_NEW || act == APP_ACT_OPEN || act == APP_ACT_OPEN_CAR || act == APP_ACT_EXPORT_PICK) dialog_start(win, a, act);
    else if (act == APP_ACT_EXPORT_AT) app_export_car(a, path);
    else if (act == APP_ACT_RECENT) app_open_recent(a, path);
    else if (act == APP_ACT_OPEN_AT) app_open_project(a, path);
    else if (act == APP_ACT_OPEN_CAR_AT) app_open_car(a, path);
    else if (act == APP_ACT_NEW_AT) app_new_project(a, path);
    return 0;
}

/* the folder the dialog gave, when it did: the project is opened (or made) in it; a cancelled dialog changes nothing */
static void take_dialog(App *a)
{
    if (g_dialog.lock == NULL) return;
    char path[APP_PATH] = "";
    AppAction kind = APP_ACT_NONE;
    SDL_LockMutex(g_dialog.lock);
    if (g_dialog.ready) {
        g_dialog.ready = 0;
        kind = g_dialog.kind;
        snprintf(path, sizeof path, "%s", g_dialog.path);
    }
    SDL_UnlockMutex(g_dialog.lock);
    if (path[0] == '\0') return;
    app_ask(a, kind == APP_ACT_NEW ? APP_ACT_NEW_AT : kind == APP_ACT_OPEN_CAR ? APP_ACT_OPEN_CAR_AT : kind == APP_ACT_EXPORT_PICK ? APP_ACT_EXPORT_AT : APP_ACT_OPEN_AT, path);   /* through the same question: what is on screen may have changed while the dialog was up */
}

/* the car of Export original car whose masks folder is `folder` (any case) chosen; 0 when there is none */
static int pick_car(App *a, const char *folder)
{
    for (int i = 0; car_export_car(i) != NULL; ++i)
        if (strcasecmp(car_export_car(i)->folder, folder) == 0) return a->export_pick = i, 1;
    return 0;
}

/* the actions asked for on the command line, once, before the first frame; 1 when all of them went */
static int run_requested(App *a, const Options *o)
{
    int ok = 1;
    if (o->save) ok &= app_save(a);
    if (o->build) ok &= app_build(a);
    if (o->install) ok &= app_install(a);
    if (o->save || o->build || o->install) fprintf(stdout, "%s\n", a->status);
    if (o->export_car != NULL) {   /* Export original car, headless: the project is made in --into and opened */
        const int made = pick_car(a, o->export_car) && o->export_into != NULL && app_export_car(a, o->export_into);
        if (!made && a->status[0] == '\0') snprintf(a->status, sizeof a->status, "--export-car %s needs a car of the list and --into PARENT", o->export_car);
        fprintf(made ? stdout : stderr, "%s\n", a->status);
        ok &= made;
    }
    if (o->changes) {   /* the masks against the game's, headless */
        const ExportCar *car = car_race_original_of(&a->car);
        char project[APP_PATH], game[APP_PATH], why[256], text[4096];
        MaskChanges *changes = malloc(sizeof *changes);
        int done = a->kind == APP_KIND_CAR && car != NULL && changes != NULL;
        if (done) {
            snprintf(project, sizeof project, "%s/car_masks/%s", a->car.piece, a->car.folder);
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
    if (o->night_track != NULL) {
        const int r = night_ref_find(o->night_track);
        if (r < 0) fprintf(stderr, "--night-track %s: not a track the night view knows\n", o->night_track);
        else a->night.ref = r;
    }
    if (o->mask != NULL) {   /* Edit mask, headless: the mask is opened, painted in rectangles, saved */
        int done = app_mask_open_by_name(a, o->mask);
        for (int i = 0; done && i < o->mask_rect_count; ++i) {
            int x, y, w, h, cls;
            if (sscanf(o->mask_rect[i], "%d,%d,%d,%d,%d", &x, &y, &w, &h, &cls) != 5 || w < 1 || h < 1) {
                snprintf(a->status, sizeof a->status, "--mask-rect wants X,Y,W,H,CLASS: %s", o->mask_rect[i]);
                done = 0;
                break;
            }
            mask_edit_stroke_begin(&a->mask.me);
            for (int yy = y; yy < y + h; ++yy)
                for (int xx = x; xx < x + w; ++xx) mask_edit_pencil(&a->mask.me, xx, yy, 1, (uint8_t)cls);
            mask_edit_stroke_end(&a->mask.me);
        }
        if (done && o->mask_save) done = app_mask_save(a);
        if (!done && a->mask.error[0] != '\0') snprintf(a->status, sizeof a->status, "%s", a->mask.error);
        fprintf(done ? stdout : stderr, "%s\n", a->status);
        ok &= done;
    }
    if (o->race_photo) {   /* Race photo, headless: the game is run and waited for, the picture is in the Race window */
        if (o->track != NULL && car_race_track_find(o->track) >= 0) a->race_track = car_race_track_find(o->track);
        a->race_night = o->night;
        int made = app_race(a, 1);
        while (made && app_race_poll(a)) SDL_Delay(20);
        made = made && a->race.ok;
        if (made && o->race_png != NULL) {
            uint8_t *png = NULL;
            size_t len = 0;
            FILE *f = NULL;
            made = png_write_rgba(a->race.photo, RACE_PHOTO_W, RACE_PHOTO_H, &png, &len) == PNG_OK && (f = fopen(o->race_png, "wb")) != NULL && fwrite(png, 1, len, f) == len;
            if (f != NULL) fclose(f);
            free(png);
            if (!made) snprintf(a->status, sizeof a->status, "cannot write %s", o->race_png);
        }
        fprintf(made ? stdout : stderr, "%s\n", a->status);
        ok &= made;
    }
    if (o->rehacer) {   /* the car's script: a headless run waits for it and says how it ended */
        ok &= app_rehacer(a);
        if (ok && o->frames > 0) {
            while (app_rehacer_poll(a)) SDL_Delay(5);
            ok &= a->rehacer.run.finished && a->rehacer.run.exit_code == 0;   /* a backup that failed never ran the script */
        }
        fprintf(ok ? stdout : stderr, "%s\n", a->status);
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
    SDL_Window *win = SDL_CreateWindow("Spirited Drive Studio", o->window_w, o->window_h, SDL_WINDOW_RESIZABLE);
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
    if (o->edit) app_add_point(a);   /* and of the confirmation: something unsaved ... */
    if (o->pick != NULL && !pick_car(a, o->pick)) {
        fprintf(stderr, "--pick %s: not a car Export original car knows\n", o->pick);
        ui_close();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 0;
    }
    if (o->ask == APP_ACT_EXPORT_PICK) a->export_dialog = 1;   /* the window itself, for a headless picture of it */
    else if (o->ask != APP_ACT_NONE) app_ask(a, o->ask, "");   /* ... and the action asked for */
    if (o->user == NULL && o->frames == 0) {   /* a normal run keeps the recent projects in the editor's own folder */
        const char *pref = SDL_GetPrefPath(NULL, "spirited-drive-studio");
        char file[APP_PATH];
        if (pref != NULL && snprintf(file, sizeof file, "%srecent.txt", pref) < (int)sizeof file) app_keep_recent(a, file);
        if (a->kind == APP_KIND_NONE && a->recent.count > 0 && recent_present(a->recent.dir[0], a->recent.kind[0])) {   /* nothing given: the project it had last */
            char last[APP_PATH];
            snprintf(last, sizeof last, "%s", a->recent.dir[0]);
            app_open_recent(a, last);
            a->open_error[0] = '\0';   /* a restore that fails is the start screen, not a message nobody asked for */
        }
    }
    const int headless = o->frames > 0;
    int ok = run_requested(a, o), frame = 0, quit = 0;
    while (!quit) {
        for (SDL_Event ev; SDL_PollEvent(&ev);) {
            ui_event(&ev);
            window_event(a, &ev);
        }
        take_dialog(a);
        if (a->reveal[0] != '\0') {   /* a folder to show in the Finder (the Masks window's button) */
            char url[APP_PATH * 3];
            if (!headless && car_export_file_url(a->reveal, url, sizeof url) && !SDL_OpenURL(url)) snprintf(a->status, sizeof a->status, "cannot open %s: %s", a->reveal, SDL_GetError());
            a->reveal[0] = '\0';
        }
        if (a->go != APP_ACT_NONE) {
            if (headless) a->go = APP_ACT_NONE;   /* a headless run ends by its frames: no dialog, no quit of its own */
            else if (run_go(win, a)) quit = 1;
        }
        app_rehacer_poll(a);
        app_race_poll(a);
        app_recompute(a);
        SDL_SetRenderDrawColor(ren, 18, 20, 24, 255);
        SDL_RenderClear(ren);
        ui_frame(ren, a);
        ++frame;
        if (headless && frame >= o->frames) {
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
    return ok;
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
    if (opened) {
        ExportSource sources;
        char *pref = o.cd == NULL && o.user == NULL ? SDL_GetPrefPath(NULL, "Spirited Drive") : NULL;   /* the game's player folder, where its own copy of the game's files is: not asked (it makes the folder) when something names it */
        car_export_source(o.game, o.user != NULL ? o.user : "", pref, o.cd, o.port_data, &sources);
        SDL_free(pref);
        app_set_export_source(app, &sources);
        app_set_game_lookup(app, tried, memo);
        app_check_game(app);   /* the window opens with what to do if the game is not found, not with a failed install later */
        status = run(app, &o) ? 0 : 1;
    } else {
        fprintf(stderr, "%s\n", app->status);
    }
    app_free(app);
    free(app);
    return status;
}
