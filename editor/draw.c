#include "draw.h"

#include "formats/png.h"
#include "formats/track_gen.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GRID_STEP = 50,            /* units between grid lines */
    POINT_HALF = 4,            /* a control point's square, half its side in pixels */
    HUD_SCALE = 2,             /* the text is drawn at twice the debug font's size */
    LINE_HEIGHT = 22,          /* pixels between text lines (a debug character is 16 pixels tall at HUD_SCALE) */
    TEXT_LINE_MAX = 160
};

typedef struct {
    Uint8 r, g, b;
} Colour;

static const Colour BACKGROUND = { 18, 20, 24 }, GRID = { 36, 40, 46 }, ROAD = { 200, 204, 212 }, BAD = { 255, 70, 70 },
                    POINT = { 90, 160, 255 }, SELECTED = { 255, 220, 0 }, TEXT = { 210, 214, 220 }, OK = { 120, 220, 140 };

/* the file name of a path (the path itself when it has no '/') */
static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static void set_colour(SDL_Renderer *ren, Colour c)
{
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
}

static void draw_grid(SDL_Renderer *ren, const View *v)
{
    double min_x, max_z, max_x, min_z;
    view_to_world(v, 0, 0, &min_x, &max_z);
    view_to_world(v, v->width, v->height, &max_x, &min_z);
    set_colour(ren, GRID);
    for (double x = floor(min_x / GRID_STEP) * GRID_STEP; x <= max_x; x += GRID_STEP) {
        double sx, sy;
        view_to_screen(v, x, 0, &sx, &sy);
        SDL_RenderLine(ren, (float)sx, 0, (float)sx, (float)v->height);
    }
    for (double z = floor(min_z / GRID_STEP) * GRID_STEP; z <= max_z; z += GRID_STEP) {
        double sx, sy;
        view_to_screen(v, 0, z, &sx, &sy);
        SDL_RenderLine(ren, 0, (float)sy, (float)v->width, (float)sy);
    }
}

static void line_between(SDL_Renderer *ren, const View *v, double x0, double z0, double x1, double z1)
{
    double ax, ay, bx, by;
    view_to_screen(v, x0, z0, &ax, &ay);
    view_to_screen(v, x1, z1, &bx, &by);
    SDL_RenderLine(ren, (float)ax, (float)ay, (float)bx, (float)by);
}

/* the road: its nodes joined, a node breaking a limit drawn red; with no road (the trace cannot make one), the control polygon in red */
static void draw_road(SDL_Renderer *ren, const App *a)
{
    const View *v = &a->view;
    if (!a->road_made) {
        set_colour(ren, BAD);
        for (int i = 1; i < a->trace.count; ++i)
            line_between(ren, v, a->trace.points[i - 1].x, a->trace.points[i - 1].z, a->trace.points[i].x, a->trace.points[i].z);
        return;
    }
    for (int i = 1; i < a->node_count; ++i) {
        const TrackGenNode *p = &a->nodes[i - 1], *q = &a->nodes[i];
        set_colour(ren, a->flags[i] ? BAD : ROAD);
        line_between(ren, v, p->position[0] / 65536.0, p->position[2] / 65536.0, q->position[0] / 65536.0, q->position[2] / 65536.0);
    }
}

static void draw_points(SDL_Renderer *ren, const App *a)
{
    for (int i = 0; i < a->trace.count; ++i) {
        double sx, sy;
        view_to_screen(&a->view, a->trace.points[i].x, a->trace.points[i].z, &sx, &sy);
        set_colour(ren, i == a->selected ? SELECTED : a->road_made ? POINT : BAD);
        const SDL_FRect box = { (float)(sx - POINT_HALF), (float)(sy - POINT_HALF), 2 * POINT_HALF, 2 * POINT_HALF };
        SDL_RenderFillRect(ren, &box);
    }
}

/* the text lines of the top-left corner: the project, the road's size and its limits */
static void draw_hud(SDL_Renderer *ren, const App *a)
{
    char line[TEXT_LINE_MAX];
    const int nodes = app_node_count(a);
    const float x = 6, y = 6;
    SDL_SetRenderScale(ren, HUD_SCALE, HUD_SCALE);
    set_colour(ren, TEXT);
    snprintf(line, sizeof line, "%s  stem %s  %s", a->project.id, a->project.stem, base_name(a->project.art));
    SDL_RenderDebugText(ren, x / HUD_SCALE, y / HUD_SCALE, line);
    snprintf(line, sizeof line, "nodes %d / %d   length %.0f u   points %d   width %.0f", nodes, TRACK_GEN_MAX_NODES, a->length,
             a->trace.count, a->trace.width);
    SDL_RenderDebugText(ren, x / HUD_SCALE, (y + LINE_HEIGHT) / HUD_SCALE, line);
    set_colour(ren, a->road_text[0] != '\0' ? BAD : OK);
    snprintf(line, sizeof line, "%s", a->road_text[0] != '\0' ? a->road_text : "limits: all good");
    SDL_RenderDebugText(ren, x / HUD_SCALE, (y + 2 * LINE_HEIGHT) / HUD_SCALE, line);
    SDL_SetRenderScale(ren, 1, 1);
}

/* the help and the status along the bottom edge */
static void draw_footer(SDL_Renderer *ren, const App *a)
{
    static const char *const help[] = {
        "drag LMB  pan RMB/MMB  zoom wheel  nudge arrows  F fit",
        "N add  I insert  Del remove  S save  B build  U install"
    };
    const float base = (float)a->view.height / HUD_SCALE - 3 * LINE_HEIGHT / HUD_SCALE;
    SDL_SetRenderScale(ren, HUD_SCALE, HUD_SCALE);
    set_colour(ren, TEXT);
    for (int i = 0; i < 2; ++i) SDL_RenderDebugText(ren, 6.0f / HUD_SCALE, base + i * LINE_HEIGHT / HUD_SCALE, help[i]);
    set_colour(ren, OK);
    SDL_RenderDebugText(ren, 6.0f / HUD_SCALE, base + 2 * LINE_HEIGHT / HUD_SCALE, a->status);
    SDL_SetRenderScale(ren, 1, 1);
}

void draw_scene(SDL_Renderer *ren, const App *a)
{
    set_colour(ren, BACKGROUND);
    SDL_RenderClear(ren);
    draw_grid(ren, &a->view);
    draw_road(ren, a);
    draw_points(ren, a);
    draw_hud(ren, a);
    draw_footer(ren, a);
}

static int save_rgba_png(SDL_Surface *surface, const char *path, char *why, size_t why_cap)
{
    const uint32_t w = (uint32_t)surface->w, h = (uint32_t)surface->h;
    uint8_t *tight = malloc((size_t)w * h * 4);
    if (tight == NULL) {
        snprintf(why, why_cap, "out of memory");
        return 0;
    }
    for (uint32_t row = 0; row < h; ++row)
        memcpy(tight + (size_t)row * w * 4, (const uint8_t *)surface->pixels + (size_t)row * (size_t)surface->pitch, (size_t)w * 4);
    uint8_t *png = NULL;
    size_t len = 0;
    const int encoded = png_write_rgba(tight, w, h, &png, &len) == PNG_OK;
    free(tight);
    if (!encoded) {
        snprintf(why, why_cap, "the picture does not encode");
        return 0;
    }
    FILE *f = fopen(path, "wb");
    const int ok = f != NULL && fwrite(png, 1, len, f) == len;
    if (f != NULL && fclose(f) != 0) snprintf(why, why_cap, "%s: cannot close it", path);
    png_free(png);
    if (!ok) snprintf(why, why_cap, "%s: cannot write it", path);
    return ok;
}

int draw_save_png(SDL_Renderer *ren, const char *path, char *why, size_t why_cap)
{
    SDL_Surface *read = SDL_RenderReadPixels(ren, NULL);
    if (read == NULL) {
        snprintf(why, why_cap, "cannot read the window: %s", SDL_GetError());
        return 0;
    }
    SDL_Surface *rgba = SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(read);
    if (rgba == NULL) {
        snprintf(why, why_cap, "cannot convert the window: %s", SDL_GetError());
        return 0;
    }
    const int ok = save_rgba_png(rgba, path, why, why_cap);
    SDL_DestroySurface(rgba);
    return ok;
}
