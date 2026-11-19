/* tnfs-track-editor: a top view of a track project's centre line (phase 1a, notes/dep59-track-editor-design.md).
 *
 *   tnfs-track-editor [PROJECT] [--game PATH] [--user DIR]
 *                     [--frames N] [--save] [--build] [--install] [--select N] [--edit] [--ask quit|new|open] [--shot OUT.png]
 *
 * PROJECT is a folder such as art/tnfs-tracks/<id>/ (created with its project.ini and trace.json when missing). The build uses no CD data:
 * the road is made on the generic template and the stage's art is the FAM named by project.ini's art=. --game and --user are for the
 * install (the game's --install; without --user, into the game's own player folder). Without PROJECT the editor opens
 * Documents/Spirited Drive Studio/my-track; without --game it takes the Spirited Drive app beside it (inside the DMG) or ./tnfs. The options after --frames run headless: N frames, then they are done, and --shot writes the last frame to a PNG; the
 * exit status is 1 when a --save, --build or --install failed. Run it with SDL_VIDEO_DRIVER=dummy and SDL_AUDIO_DRIVER=dummy for a headless
 * run. */
#include "app.h"
#include "art.h"
#include "build.h"
#include "capture.h"
#include "game_find.h"
#include "ui.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    WINDOW_W = 960,
    WINDOW_H = 600,
    FRAME_MS = 16
};

typedef struct {
    const char *dir, *game, *user, *shot;
    int frames, save, build, install, select;   /* select: a control point to select before the first frame, -1 for none */
    int edit;                /* a point added before the first frame: unsaved changes, for a headless picture of the confirmation */
    AppAction ask;           /* an action asked for before the first frame (quit, new, open) */
} Options;

static int usage(void)
{
    fprintf(stderr, "usage: tnfs-track-editor [PROJECT] [--game PATH] [--user DIR] [--frames N] [--save] [--build] "
                    "[--install] [--select N] [--edit] [--ask quit|new|open] [--shot OUT.png]\n");
    return 0;
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    o->select = -1;
    for (int i = 1; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--game") == 0 && has_value) o->game = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && has_value) o->user = argv[++i];
        else if (strcmp(argv[i], "--shot") == 0 && has_value) o->shot = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && has_value) o->frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--select") == 0 && has_value) o->select = atoi(argv[++i]);
        else if (strcmp(argv[i], "--ask") == 0 && has_value) {
            const char *what = argv[++i];
            o->ask = strcmp(what, "quit") == 0 ? APP_ACT_QUIT : strcmp(what, "new") == 0 ? APP_ACT_NEW : strcmp(what, "open") == 0 ? APP_ACT_OPEN : APP_ACT_NONE;
            if (o->ask == APP_ACT_NONE) return usage();
        } else if (strcmp(argv[i], "--edit") == 0) o->edit = 1;
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

/* The project folder when none is given (the app opened from the Finder): Documents/Spirited Drive Studio/my-track, made if missing. */
static int default_project(char *out, size_t cap)
{
    const char *docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    if (docs == NULL) return 0;
    const int n = snprintf(out, cap, "%sSpirited Drive Studio/my-track", docs);
    return n > 0 && (size_t)n < cap && SDL_CreateDirectory(out);
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
    AppAction kind;          /* APP_ACT_NEW or APP_ACT_OPEN */
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
    snprintf(where, sizeof where, "%s", a->dir);
    char *slash = strrchr(where, '/');
    if (slash != NULL && slash != where) *slash = '\0';
    g_dialog.open = 1;
    g_dialog.ready = 0;
    g_dialog.kind = kind;
    SDL_ShowOpenFolderDialog(dialog_done, &g_dialog, win, where, false);
}

/* what to do with an action that is ready to go (confirmed, or nothing was unsaved); 1 to quit */
static int run_go(SDL_Window *win, App *a)
{
    const AppAction act = a->go;
    char path[APP_PATH];
    snprintf(path, sizeof path, "%s", a->go_path);
    a->go = APP_ACT_NONE;
    if (act == APP_ACT_QUIT) return 1;
    if (act == APP_ACT_NEW || act == APP_ACT_OPEN) dialog_start(win, a, act);
    else if (act == APP_ACT_RECENT || act == APP_ACT_OPEN_AT) app_open_project(a, path);
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
    app_ask(a, kind == APP_ACT_NEW ? APP_ACT_NEW_AT : APP_ACT_OPEN_AT, path);   /* through the same question: what is on screen may have changed while the dialog was up */
}

/* the actions asked for on the command line, once, before the first frame; 1 when all of them went */
static int run_requested(App *a, const Options *o)
{
    int ok = 1;
    if (o->save) ok &= app_save(a);
    if (o->build) ok &= app_build(a);
    if (o->install) ok &= app_install(a);
    if (o->save || o->build || o->install) fprintf(stdout, "%s\n", a->status);
    return ok;
}

static int run(App *a, const Options *o)
{
    if (o->shot != NULL) SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("Spirited Drive: track editor", WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
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
    if (o->select >= 0 && o->select < a->trace.count) a->selected = o->select;   /* for a headless picture of the Node window */
    if (o->edit) app_add_point(a);   /* and of the confirmation: something unsaved ... */
    if (o->ask != APP_ACT_NONE) app_ask(a, o->ask, "");   /* ... and the action asked for */
    if (o->user == NULL && o->frames == 0) {   /* a normal run keeps the recent projects in the editor's own folder */
        const char *pref = SDL_GetPrefPath(NULL, "spirited-drive-studio");
        char file[APP_PATH];
        if (pref != NULL && snprintf(file, sizeof file, "%srecent.txt", pref) < (int)sizeof file) app_keep_recent(a, file);
    }
    const int headless = o->frames > 0;
    int ok = run_requested(a, o), frame = 0, quit = 0;
    while (!quit) {
        for (SDL_Event ev; SDL_PollEvent(&ev);) {
            ui_event(&ev);
            window_event(a, &ev);
        }
        take_dialog(a);
        if (a->go != APP_ACT_NONE) {
            if (headless) a->go = APP_ACT_NONE;   /* a headless run ends by its frames: no dialog, no quit of its own */
            else if (run_go(win, a)) quit = 1;
        }
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
    char game[APP_PATH], project[APP_PATH], tried[GAME_FIND_TRIED] = "", memo[APP_PATH] = "";
    if (o.game == NULL) {
        default_game(game, sizeof game, tried, sizeof tried, memo, sizeof memo);
        o.game = game;
    }
    if (o.dir == NULL) {
        if (!default_project(project, sizeof project)) {
            fprintf(stderr, "no project folder given and Documents/Spirited Drive Studio cannot be made: %s\n", SDL_GetError());
            return 2;
        }
        o.dir = project;
    }
    App *app = calloc(1, sizeof *app);
    if (app == NULL) return 1;
    int status = 1;
    char generic[APP_PATH];
    if (app_open(app, o.dir, o.game, o.user != NULL ? o.user : "", art_generic_find(generic, sizeof generic) ? generic : NULL)) {
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
