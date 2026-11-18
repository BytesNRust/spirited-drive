/* tnfs-track-editor: a top view of a track project's centre line (phase 1a, notes/dep59-track-editor-design.md).
 *
 *   tnfs-track-editor PROJECT [--data DIR] [--game PATH] [--user DIR]
 *                     [--frames N] [--save] [--build] [--install] [--shot OUT.png]
 *
 * PROJECT is a folder such as art/tnfs-tracks/<id>/ (created with its project.ini and trace.json when missing). --data is the CD's folder
 * (default original/): the build takes the template track from it. --game and --user are for the install (the game's --install into the
 * player folder). The options after --frames run headless: N frames, then they are done, and --shot writes the last frame to a PNG; the
 * exit status is 1 when a --save, --build or --install failed. Run it with SDL_VIDEO_DRIVER=dummy and SDL_AUDIO_DRIVER=dummy for a headless
 * run. */
#include "app.h"
#include "build.h"
#include "draw.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    WINDOW_W = 960,
    WINDOW_H = 600,
    FRAME_MS = 16,
    PICK_REACH = 12,         /* pixels a click may be from a point and still take it */
    NUDGE = 1                /* units an arrow key moves the selected point */
};

typedef struct {
    const char *dir, *data, *game, *user, *shot;
    int frames, save, build, install;
} Options;

typedef struct {
    int dragging, panning, quit;
    float last_x, last_y;
} Pointer;

static int usage(void)
{
    fprintf(stderr, "usage: tnfs-track-editor PROJECT [--data DIR] [--game PATH] [--user DIR] [--frames N] [--save] [--build] "
                    "[--install] [--shot OUT.png]\n");
    return 0;
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    o->data = "original";
    o->game = "./tnfs";
    for (int i = 1; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--data") == 0 && has_value) o->data = argv[++i];
        else if (strcmp(argv[i], "--game") == 0 && has_value) o->game = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && has_value) o->user = argv[++i];
        else if (strcmp(argv[i], "--shot") == 0 && has_value) o->shot = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && has_value) o->frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--save") == 0) o->save = 1;
        else if (strcmp(argv[i], "--build") == 0) o->build = 1;
        else if (strcmp(argv[i], "--install") == 0) o->install = 1;
        else if (argv[i][0] != '-' && o->dir == NULL) o->dir = argv[i];
        else return usage();
    }
    if (o->dir == NULL) return usage();
    if (o->install && o->user == NULL) {
        fprintf(stderr, "--install needs --user DIR (the player folder)\n");
        return 0;
    }
    return 1;
}

static void nudge(App *a, double dx, double dz)
{
    if (a->selected < 0) return;
    a->trace.points[a->selected].x += dx;
    a->trace.points[a->selected].z += dz;
    a->dirty = 1;
}

static void zoom_at(App *a, double sx, double sy, double factor)
{
    double before_x, before_z, after_x, after_z;
    view_to_world(&a->view, sx, sy, &before_x, &before_z);
    const double scale = a->view.scale * factor;
    a->view.scale = scale < VIEW_SCALE_MIN ? VIEW_SCALE_MIN : scale > VIEW_SCALE_MAX ? VIEW_SCALE_MAX : scale;
    view_to_world(&a->view, sx, sy, &after_x, &after_z);
    a->view.cx += before_x - after_x;
    a->view.cz += before_z - after_z;
}

static void on_key(App *a, SDL_Keycode key)
{
    switch (key) {
    case SDLK_ESCAPE:
    case SDLK_Q: break;   /* handled by the caller */
    case SDLK_N: app_add_point(a); break;
    case SDLK_I: app_insert_point(a); break;
    case SDLK_DELETE:
    case SDLK_X: app_remove_point(a); break;
    case SDLK_S: app_save(a); break;
    case SDLK_B: app_build(a); break;
    case SDLK_U: app_install(a); break;
    case SDLK_F: view_fit(&a->view, a->trace.points, a->trace.count, a->view.width, a->view.height); break;
    case SDLK_EQUALS:
    case SDLK_KP_PLUS: zoom_at(a, a->view.width / 2.0, a->view.height / 2.0, VIEW_ZOOM_STEP); break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS: zoom_at(a, a->view.width / 2.0, a->view.height / 2.0, 1 / VIEW_ZOOM_STEP); break;
    case SDLK_LEFT: nudge(a, -NUDGE, 0); break;
    case SDLK_RIGHT: nudge(a, NUDGE, 0); break;
    case SDLK_UP: nudge(a, 0, NUDGE); break;
    case SDLK_DOWN: nudge(a, 0, -NUDGE); break;
    default: break;
    }
}

static void on_button_down(App *a, Pointer *p, const SDL_MouseButtonEvent *b)
{
    p->last_x = b->x;
    p->last_y = b->y;
    if (b->button == SDL_BUTTON_LEFT) {
        a->selected = view_pick(&a->view, a->trace.points, a->trace.count, b->x, b->y, PICK_REACH);
        p->dragging = a->selected >= 0;
    } else if (b->button == SDL_BUTTON_RIGHT || b->button == SDL_BUTTON_MIDDLE) {
        p->panning = 1;
    }
}

static void on_motion(App *a, Pointer *p, const SDL_MouseMotionEvent *m)
{
    if (p->dragging && a->selected >= 0) {
        double x, z;
        view_to_world(&a->view, m->x, m->y, &x, &z);
        a->trace.points[a->selected].x = x;
        a->trace.points[a->selected].z = z;
        a->dirty = 1;
    }
    if (p->panning) {
        a->view.cx -= (m->x - p->last_x) / a->view.scale;
        a->view.cz += (m->y - p->last_y) / a->view.scale;
    }
    p->last_x = m->x;
    p->last_y = m->y;
}

static void on_event(App *a, Pointer *p, const SDL_Event *ev)
{
    switch (ev->type) {
    case SDL_EVENT_QUIT: p->quit = 1; break;
    case SDL_EVENT_KEY_DOWN:
        if (ev->key.key == SDLK_ESCAPE) p->quit = 1;
        else if (!ev->key.repeat) on_key(a, ev->key.key);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: on_button_down(a, p, &ev->button); break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.button == SDL_BUTTON_LEFT) p->dragging = 0;
        else p->panning = 0;
        break;
    case SDL_EVENT_MOUSE_MOTION: on_motion(a, p, &ev->motion); break;
    case SDL_EVENT_MOUSE_WHEEL:
        zoom_at(a, ev->wheel.mouse_x, ev->wheel.mouse_y, ev->wheel.y > 0 ? VIEW_ZOOM_STEP : 1 / VIEW_ZOOM_STEP);
        break;
    default: break;
    }
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
    int w = WINDOW_W, h = WINDOW_H;
    SDL_GetWindowSize(win, &w, &h);
    view_fit(&a->view, a->trace.points, a->trace.count, w, h);
    int ok = run_requested(a, o), frame = 0;
    Pointer p;
    memset(&p, 0, sizeof p);
    const int headless = o->frames > 0;
    while (!p.quit) {
        for (SDL_Event ev; SDL_PollEvent(&ev);) on_event(a, &p, &ev);
        SDL_GetWindowSize(win, &a->view.width, &a->view.height);
        app_recompute(a);
        draw_scene(ren, a);
        ++frame;
        if (headless && frame >= o->frames) {
            if (o->shot != NULL) {
                char why[APP_MSG];
                if (!draw_save_png(ren, o->shot, why, sizeof why)) {
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
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return ok;
}

int main(int argc, char **argv)
{
    Options o;
    if (!parse_args(argc, argv, &o)) return 2;
    App *app = calloc(1, sizeof *app);
    if (app == NULL) return 1;
    int status = 1;
    if (app_open(app, o.dir, o.data, o.game, o.user)) {
        status = run(app, &o) ? 0 : 1;
    } else {
        fprintf(stderr, "%s\n", app->status);
    }
    app_free(app);
    free(app);
    return status;
}
