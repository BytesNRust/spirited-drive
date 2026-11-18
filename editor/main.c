/* tnfs-track-editor: a top view of a track project's centre line (phase 1a, notes/dep59-track-editor-design.md).
 *
 *   tnfs-track-editor [PROJECT] [--game PATH] [--user DIR]
 *                     [--frames N] [--save] [--build] [--install] [--shot OUT.png]
 *
 * PROJECT is a folder such as art/tnfs-tracks/<id>/ (created with its project.ini and trace.json when missing). The build uses no CD data:
 * the road is made on the generic template and the stage's art is the FAM named by project.ini's art=. --game and --user are for the
 * install (the game's --install; without --user, into the game's own player folder). Without PROJECT the editor opens
 * Documents/Spirited Drive Studio/my-track; without --game it takes the Spirited Drive app beside it (inside the DMG) or ./tnfs. The options after --frames run headless: N frames, then they are done, and --shot writes the last frame to a PNG; the
 * exit status is 1 when a --save, --build or --install failed. Run it with SDL_VIDEO_DRIVER=dummy and SDL_AUDIO_DRIVER=dummy for a headless
 * run. */
#include "app.h"
#include "build.h"
#include "capture.h"
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
    int frames, save, build, install;
} Options;

static int usage(void)
{
    fprintf(stderr, "usage: tnfs-track-editor [PROJECT] [--game PATH] [--user DIR] [--frames N] [--save] [--build] "
                    "[--install] [--shot OUT.png]\n");
    return 0;
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    for (int i = 1; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--game") == 0 && has_value) o->game = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && has_value) o->user = argv[++i];
        else if (strcmp(argv[i], "--shot") == 0 && has_value) o->shot = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && has_value) o->frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--save") == 0) o->save = 1;
        else if (strcmp(argv[i], "--build") == 0) o->build = 1;
        else if (strcmp(argv[i], "--install") == 0) o->install = 1;
        else if (argv[i][0] != '-' && o->dir == NULL) o->dir = argv[i];
        else return usage();
    }
    return 1;
}

/* The game's program, when --game is not given: the app beside this one (the DMG's "Spirited Drive.app" next to "Spirited Drive Studio.app",
 * in the Applications folder or anywhere both were dragged), else ./tnfs as a build tree has it. */
static void default_game(char *out, size_t cap)
{
    static const char APP_TNFS[] = "Spirited Drive.app/Contents/MacOS/tnfs";
    const char *base = SDL_GetBasePath();   /* ".../Spirited Drive Studio.app/Contents/Resources/" inside the bundle */
    char sibling[APP_PATH];
    SDL_PathInfo info;
    if (base != NULL) {
        snprintf(sibling, sizeof sibling, "%s../../../%s", base, APP_TNFS);
        if (SDL_GetPathInfo(sibling, &info) && info.type == SDL_PATHTYPE_FILE) {
            snprintf(out, cap, "%s", sibling);
            return;
        }
    }
    snprintf(sibling, sizeof sibling, "/Applications/%s", APP_TNFS);
    snprintf(out, cap, "%s", SDL_GetPathInfo(sibling, &info) && info.type == SDL_PATHTYPE_FILE ? sibling : "./tnfs");
}

/* The project folder when none is given (the app opened from the Finder): Documents/Spirited Drive Studio/my-track, made if missing. */
static int default_project(char *out, size_t cap)
{
    const char *docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    if (docs == NULL) return 0;
    const int n = snprintf(out, cap, "%sSpirited Drive Studio/my-track", docs);
    return n > 0 && (size_t)n < cap && SDL_CreateDirectory(out);
}

/* the window's own events: closing it, or Escape (which no panel keeps) */
static int wants_quit(const SDL_Event *ev)
{
    return ev->type == SDL_EVENT_QUIT || (ev->type == SDL_EVENT_KEY_DOWN && ev->key.key == SDLK_ESCAPE);
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
    int ok = run_requested(a, o), frame = 0, quit = 0;
    const int headless = o->frames > 0;
    while (!quit) {
        for (SDL_Event ev; SDL_PollEvent(&ev);) {
            ui_event(&ev);
            if (wants_quit(&ev)) quit = 1;
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
    char game[APP_PATH], project[APP_PATH];
    if (o.game == NULL) {
        default_game(game, sizeof game);
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
    if (app_open(app, o.dir, o.game, o.user != NULL ? o.user : "")) {
        status = run(app, &o) ? 0 : 1;
    } else {
        fprintf(stderr, "%s\n", app->status);
    }
    app_free(app);
    free(app);
    return status;
}
