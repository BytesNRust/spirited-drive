/* port (wipers, dep49): see dash_nose.h. */
#include "game/race/dash_nose.h"
#include "game/race/dash_mask.h"
#include "game/sys/port_follow.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { SIDE_GLASS = 0, SIDE_NOSE = 1, STRAIGHT = 3, DIAGONAL = 4, MAX_SIDE = 4096, MOST_SWEEPS = 4000 };
static const float SETTLED = 0.01f;     /* a sweep that moves no channel more than this ends the fill */
static const float OVER_RELAX = 1.85f;  /* successive over-relaxation: the same solution, in far fewer sweeps */

static const int DX[8] = { -1, 1, 0, 0, -1, 1, -1, 1 }, DY[8] = { 0, 0, -1, 1, -1, -1, 1, 1 };

/* ----- which red pixels are the nose's: the side of the nearest pixel that is not red (Dijkstra over the red, chamfer 3-4) ----- */

typedef struct {
    int64_t *items;   /* (rank << 32) | pixel: rank is the distance doubled, plus 1 for the nose's side, so at the same distance the glass's side comes first */
    size_t count, cap;
} Heap;

static int heap_push(Heap *h, int64_t v)
{
    if (h->count == h->cap) {
        const size_t cap = h->cap != 0 ? h->cap * 2 : 1024;
        int64_t *grown = realloc(h->items, cap * sizeof *grown);
        if (grown == NULL) return 0;
        h->items = grown, h->cap = cap;
    }
    size_t i = h->count++;
    while (i > 0 && h->items[(i - 1) / 2] > v) h->items[i] = h->items[(i - 1) / 2], i = (i - 1) / 2;
    h->items[i] = v;
    return 1;
}

static int64_t heap_pop(Heap *h)
{
    const int64_t top = h->items[0], last = h->items[--h->count];
    size_t i = 0;
    for (;;) {
        size_t c = 2 * i + 1;
        if (c >= h->count) break;
        if (c + 1 < h->count && h->items[c + 1] < h->items[c]) c++;
        if (h->items[c] >= last) break;
        h->items[i] = h->items[c], i = c;
    }
    if (h->count > 0) h->items[i] = last;
    return top;
}

typedef struct {
    const uint8_t *art, *mask;
    int w, h;
    uint8_t key;
} Plane;

static int is_red(const Plane *p, int i) { return p->mask[i] == DASH_MASK_BLADE && p->art[i] != p->key; }
static int is_nose(const Plane *p, int i) { return p->mask[i] == DASH_MASK_NOSE && p->art[i] != p->key; }
static int is_glass(const Plane *p, int i) { return p->art[i] == p->key; }

/* rank[i] for every red pixel (INT_MAX: no side reached it) and origin[i], the pixel that is not red it was reached from; 0 when there is no memory */
static int red_sides(const Plane *p, int *rank, int *origin)
{
    const int n = p->w * p->h;
    Heap heap = { NULL, 0, 0 };
    for (int i = 0; i < n; i++) rank[i] = INT_MAX, origin[i] = -1;
    for (int i = 0; i < n; i++) {   /* the sources: the glass and the nose next to the red; the dash's other art claims nothing (a blade on the nose that touches it is still the nose's) */
        if (!is_glass(p, i) && !is_nose(p, i)) continue;
        const int x = i % p->w, y = i / p->w;
        for (int k = 0; k < 8; k++) {
            const int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || nx >= p->w || ny < 0 || ny >= p->h || !is_red(p, ny * p->w + nx)) continue;
            const int r = (k < 4 ? STRAIGHT : DIAGONAL) * 2 + (is_nose(p, i) ? SIDE_NOSE : SIDE_GLASS), at = ny * p->w + nx;
            if (r < rank[at]) {
                rank[at] = r, origin[at] = i;
                if (!heap_push(&heap, (int64_t)r << 32 | at)) return free(heap.items), 0;
            }
        }
    }
    while (heap.count > 0) {
        const int64_t v = heap_pop(&heap);
        const int at = (int)(v & 0xffffffff), r = (int)(v >> 32);
        if (r != rank[at]) continue;   /* a stale entry: the pixel was reached closer since */
        const int x = at % p->w, y = at / p->w, side = r & 1;
        for (int k = 0; k < 8; k++) {
            const int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || nx >= p->w || ny < 0 || ny >= p->h) continue;
            const int q = ny * p->w + nx, nr = ((r >> 1) + (k < 4 ? STRAIGHT : DIAGONAL)) * 2 + side;
            if (!is_red(p, q) || nr >= rank[q]) continue;
            rank[q] = nr, origin[q] = origin[at];
            if (!heap_push(&heap, (int64_t)nr << 32 | q)) return free(heap.items), 0;
        }
    }
    free(heap.items);
    return 1;
}

/* ----- the fill ----- */

/* The hood's own slope round a piece of blade: a plane per channel (v = a + b x + c y), least squares over the nose pixels within RING of the piece. Laplace's equation is solved
 * for what the plane leaves (the residual), so a straight gradient comes out exact on both sides of a blade and carries on where the blade has the nose on one side only (along
 * the hood's edge, at the picture's edge), and the residual still bends the fill to the nose's own pixels round it. */
enum { RING = 4 };

typedef struct {
    float a[3], b[3], c[3];
    float lo[3], hi[3];   /* the ring's own range per channel: the fill never goes past the colours round it (a plane carried far from its ring could) */
} Slope;

static float slope_at(const Slope *s, int ch, int x, int y) { return s->a[ch] + s->b[ch] * (float)x + s->c[ch] * (float)y; }

/* the plane through the sums (n, x, y, xx, xy, yy; v, xv, yv per channel); flat at the mean when the points do not span one */
static Slope slope_solve(const double m[6], const double v[3][3])
{
    Slope s;
    const double n = m[0], sx = m[1], sy = m[2], xx = m[3], xy = m[4], yy = m[5];
    const double det = n * (xx * yy - xy * xy) - sx * (sx * yy - xy * sy) + sy * (sx * xy - xx * sy);
    for (int ch = 0; ch < 3; ch++) {
        if (n <= 0.0) {
            s.a[ch] = s.b[ch] = s.c[ch] = 0.0f;
            continue;
        }
        if (fabs(det) < 1e-6 * n * n * n) {   /* a line or a point: flat */
            s.a[ch] = (float)(v[ch][0] / n), s.b[ch] = s.c[ch] = 0.0f;
            continue;
        }
        const double r0 = v[ch][0], r1 = v[ch][1], r2 = v[ch][2];   /* Cramer's rule on the normal equations */
        s.a[ch] = (float)((r0 * (xx * yy - xy * xy) - sx * (r1 * yy - xy * r2) + sy * (r1 * xy - xx * r2)) / det);
        s.b[ch] = (float)((n * (r1 * yy - xy * r2) - r0 * (sx * yy - xy * sy) + sy * (sx * r2 - r1 * sy)) / det);
        s.c[ch] = (float)((n * (xx * r2 - r1 * xy) - sx * (sx * r2 - r1 * sy) + r0 * (sx * xy - xx * sy)) / det);
    }
    return s;
}

/* The pieces of blade on the nose (8-connected) into piece[] (-1 elsewhere) and each piece's slope; `seen` is work area. The number of pieces, -1 with no memory. */
static int pieces_and_slopes(const Plane *p, const int *todo, int count, const uint8_t *on, const float *rgb, int *piece, int *seen, Slope **slopes)
{
    const int n = p->w * p->h;
    for (int i = 0; i < n; i++) piece[i] = -1, seen[i] = -1;
    int pieces = 0;
    int *const stack = malloc(sizeof(int) * (size_t)(count > 0 ? count : 1));
    Slope *list = NULL;
    if (stack == NULL) return -1;
    for (int t = 0; t < count; t++) {
        if (piece[todo[t]] >= 0) continue;
        Slope *const grown = realloc(list, sizeof(Slope) * (size_t)(pieces + 1));
        if (grown == NULL) return free(stack), free(list), -1;
        list = grown;
        double m[6] = { 0 }, v[3][3] = { { 0 } };
        float lo[3] = { 255.0f, 255.0f, 255.0f }, hi[3] = { 0.0f, 0.0f, 0.0f };
        int top = 0;
        stack[top++] = todo[t], piece[todo[t]] = pieces;
        while (top > 0) {
            const int at = stack[--top], x = at % p->w, y = at / p->w;
            for (int k = 0; k < 8; k++) {   /* the piece's own pixels */
                const int nx = x + DX[k], ny = y + DY[k];
                if (nx < 0 || nx >= p->w || ny < 0 || ny >= p->h) continue;
                const int q = ny * p->w + nx;
                if (piece[q] < 0 && on[q] && is_red(p, q)) piece[q] = pieces, stack[top++] = q;
            }
            for (int dy = -RING; dy <= RING; dy++)   /* the nose round it, each pixel once a piece */
                for (int dx = -RING; dx <= RING; dx++) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= p->w || ny < 0 || ny >= p->h) continue;
                    const int q = ny * p->w + nx;
                    if (seen[q] == pieces || !is_nose(p, q)) continue;
                    seen[q] = pieces;
                    m[0] += 1.0, m[1] += nx, m[2] += ny, m[3] += (double)nx * nx, m[4] += (double)nx * ny, m[5] += (double)ny * ny;
                    for (int ch = 0; ch < 3; ch++) {
                        const double c = rgb[3 * q + ch];
                        v[ch][0] += c, v[ch][1] += c * nx, v[ch][2] += c * ny;
                        lo[ch] = fminf(lo[ch], rgb[3 * q + ch]), hi[ch] = fmaxf(hi[ch], rgb[3 * q + ch]);
                    }
                }
        }
        list[pieces] = slope_solve(m, v);
        for (int ch = 0; ch < 3; ch++) list[pieces].lo[ch] = lo[ch] <= hi[ch] ? lo[ch] : 0.0f, list[pieces].hi[ch] = lo[ch] <= hi[ch] ? hi[ch] : 255.0f;
        pieces++;
    }
    free(stack);
    *slopes = list;
    return pieces;
}

/* Gauss-Seidel with over-relaxation over the unknowns `todo` (`count` of them) on the residual of their piece's slope: rgb holds every pixel's colour on the nose (known, and
 * the unknowns' first guess), `on` flags them; the unknowns' colours are written back to rgb */
static void relax(const Plane *p, const int *todo, int count, const uint8_t *on, const int *piece, const Slope *slopes, float *rgb)
{
    for (int t = 0; t < count; t++) {   /* the unknowns as residuals */
        const int i = todo[t];
        for (int ch = 0; ch < 3; ch++) rgb[3 * i + ch] -= slope_at(&slopes[piece[i]], ch, i % p->w, i / p->w);
    }
    for (int sweep = 0; sweep < MOST_SWEEPS; sweep++) {
        float moved = 0.0f;
        for (int t = 0; t < count; t++) {
            const int i = todo[t], x = i % p->w, y = i / p->w;
            const Slope *const s = &slopes[piece[i]];
            float sum[3] = { 0.0f, 0.0f, 0.0f };
            int n = 0;
            for (int k = 0; k < 4; k++) {   /* the four neighbours on the nose: the glass, the dash's art and the road are never read */
                const int nx = x + DX[k], ny = y + DY[k];
                if (nx < 0 || nx >= p->w || ny < 0 || ny >= p->h || !on[ny * p->w + nx]) continue;
                const int q = ny * p->w + nx;
                const int unknown = piece[q] >= 0;   /* an unknown neighbour holds its residual already (the same piece: 4 neighbours are 8-connected) */
                for (int ch = 0; ch < 3; ch++) sum[ch] += unknown ? rgb[3 * q + ch] : rgb[3 * q + ch] - slope_at(s, ch, nx, ny);
                n++;
            }
            if (n == 0) continue;
            for (int ch = 0; ch < 3; ch++) {
                const float step = OVER_RELAX * (sum[ch] / (float)n - rgb[3 * i + ch]);
                rgb[3 * i + ch] += step;
                if (fabsf(step) > moved) moved = fabsf(step);
            }
        }
        if (moved < SETTLED) break;
    }
    for (int t = 0; t < count; t++) {   /* back to colours, within the ring's range */
        const int i = todo[t];
        const Slope *const s = &slopes[piece[i]];
        for (int ch = 0; ch < 3; ch++) rgb[3 * i + ch] = fminf(fmaxf(rgb[3 * i + ch] + slope_at(s, ch, i % p->w, i / p->w), s->lo[ch]), s->hi[ch]);
    }
}

/* the 4 x 4 ordered dither's thresholds (sixteenths) */
static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

static float distance2(const float *a, const float *b) { return (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]); }

/* A filled pixel's colour at (x, y) among the nose's own colours (`used`): the nearest, or, when the line from it to another of them passes nearer, one of that pair by the
 * ordered dither's threshold there, so a textured hood (its ramp dithered with a second colour) is made again as a mix of its colours, not one flat entry. */
static uint8_t pick_colour(const uint8_t palette[768], const uint8_t used[256], const float *rgb, int x, int y)
{
    float col[256][3], best = 0.0f;
    int a = -1;
    for (int k = 0; k < 256; k++) {
        if (!used[k]) continue;
        col[k][0] = palette[3 * k], col[k][1] = palette[3 * k + 1], col[k][2] = palette[3 * k + 2];
        const float d = distance2(rgb, col[k]);
        if (a < 0 || d < best) best = d, a = k;
    }
    int b = -1;
    float share = 0.0f;   /* how far along a -> b */
    for (int k = 0; k < 256; k++) {
        if (!used[k] || k == a) continue;
        const float e[3] = { col[k][0] - col[a][0], col[k][1] - col[a][1], col[k][2] - col[a][2] }, len2 = e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
        if (len2 <= 0.0f) continue;
        float t = ((rgb[0] - col[a][0]) * e[0] + (rgb[1] - col[a][1]) * e[1] + (rgb[2] - col[a][2]) * e[2]) / len2;
        t = fminf(fmaxf(t, 0.0f), 1.0f);
        const float on[3] = { col[a][0] + t * e[0], col[a][1] + t * e[1], col[a][2] + t * e[2] }, d = distance2(rgb, on);
        if (t > 0.0f && d < best) best = d, b = k, share = t;
    }
    return (uint8_t)(b >= 0 && share * 16.0f > (float)BAYER[y & 3][x & 3] + 0.5f ? b : a);
}

int dash_nose_fill(const uint8_t *art, const uint8_t *mask, int width, int height, const uint8_t palette[768], uint8_t key, uint8_t *out)
{
    if (art == NULL || mask == NULL || palette == NULL || out == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return -1;
    const Plane p = { art, mask, width, height, key };
    const int n = width * height;
    memset(out, key, (size_t)n);
    uint8_t used[256] = { 0 };
    int known = 0;
    for (int i = 0; i < n; i++)
        if (is_nose(&p, i)) out[i] = art[i], used[art[i]] = 1, known++;
    if (known == 0) return 0;
    int *const rank = malloc(sizeof(int) * (size_t)n), *const origin = malloc(sizeof(int) * (size_t)n), *const todo = malloc(sizeof(int) * (size_t)n);
    uint8_t *const on = malloc((size_t)n);
    float *const rgb = malloc(sizeof(float) * 3 * (size_t)n);
    int result = -1;
    if (rank != NULL && origin != NULL && todo != NULL && on != NULL && rgb != NULL && red_sides(&p, rank, origin)) {
        int count = 0;
        for (int i = 0; i < n; i++) {
            const int filled = is_red(&p, i) && rank[i] != INT_MAX && (rank[i] & 1) == SIDE_NOSE;
            on[i] = (uint8_t)(is_nose(&p, i) || filled);
            if (!on[i]) continue;
            const uint8_t from = filled ? art[origin[i]] : art[i];   /* a filled pixel starts as the nose pixel it was reached from: few sweeps from there */
            for (int c = 0; c < 3; c++) rgb[3 * i + c] = (float)palette[3 * from + c];
            if (filled) todo[count++] = i;
        }
        /* rank and origin are done with: they hold the pieces and the work of their slopes */
        Slope *slopes = NULL;
        int *const piece = rank, *const seen = origin;
        if (pieces_and_slopes(&p, todo, count, on, rgb, piece, seen, &slopes) >= 0) {
            relax(&p, todo, count, on, piece, slopes, rgb);
            for (int t = 0; t < count; t++) out[todo[t]] = pick_colour(palette, used, rgb + 3 * todo[t], todo[t] % width, todo[t] / width);
            result = known + count;
        }
        free(slopes);
    }
    free(rank), free(origin), free(todo), free(on), free(rgb);
    if (result < 0) memset(out, key, (size_t)n);
    return result;
}

/* ----- the cache ----- */

DashNoseKey dash_nose_key(const void *art_id, const void *mask_id, const uint8_t palette[768])
{
    DashNoseKey k;
    memset(&k, 0, sizeof k);
    k.art_id = art_id, k.mask_id = mask_id;
    if (palette != NULL) memcpy(k.palette, palette, sizeof k.palette);
    k.generation = port_settings_generation();
    k.wipers = palette != NULL && port_wipers();
    return k;
}

void dash_nose_cache_free(DashNoseCache *c)
{
    free(c->pixels);
    c->pixels = NULL, c->made = 0, c->count = 0;
}

const uint8_t *dash_nose_cached(DashNoseCache *c, const DashNoseKey *key, const uint8_t *art, const uint8_t *mask, int width, int height, uint8_t key_colour)
{
    if (key == NULL || !key->wipers || art == NULL || mask == NULL) {   /* off: nothing kept, the next nose is made afresh */
        if (c->made) dash_nose_cache_free(c);
        return NULL;
    }
    if (!c->made || memcmp(&c->key, key, sizeof *key) != 0) {
        uint8_t *const grown = realloc(c->pixels, (size_t)width * (size_t)height);
        if (grown == NULL) return NULL;
        c->pixels = grown;
        const int count = dash_nose_fill(art, mask, width, height, key->palette, key_colour, c->pixels);
        if (count < 0) {   /* no memory: said, nothing kept, tried again next frame */
            port_log("dash nose: cannot make the nose (%dx%d)\n", width, height);
            c->made = 0, c->count = 0;
            return NULL;
        }
        c->count = count;
        c->key = *key, c->made = 1, c->builds++;
    }
    return c->count > 0 ? c->pixels : NULL;
}
