#include "game/viewer/halo_track.h"

#include <stdlib.h>
#include <string.h>

enum { MATCH_BASE = 4, MATCH_RADIUS_DIV = 4 };
static const float SMOOTH = 0.4f, AREA_SMOOTH = 0.3f, ESTABLISHED = 1.5f;   /* the radius' step toward the blob's; a shown halo's weight against a new one's */

static int before(const HaloCand *a, const HaloCand *b)
{
    if (a->area != b->area) return a->area > b->area;
    if ((a->depth != 0) != (b->depth != 0)) return a->depth != 0;
    if (a->depth != b->depth) return a->depth < b->depth;
    if (a->cy != b->cy) return a->cy < b->cy;
    return a->cx < b->cx;
}

static int by_priority(const void *p, const void *q)
{
    const HaloCand *a = p, *b = q;
    return before(a, b) ? -1 : before(b, a) ? 1 : 0;
}

void halo_track_clear(HaloTracker *tracker)
{
    memset(tracker, 0, sizeof *tracker);
}

static float score(const HaloTrack *t)
{
    return t->area_avg * (t->shown ? ESTABLISHED : 1.0f) + (t->shown ? 0.25f : 0.0f);
}

static int by_score(const void *p, const void *q)
{
    const HaloTrack *a = *(const HaloTrack *const *)p, *b = *(const HaloTrack *const *)q;
    const float sa = score(a), sb = score(b);
    if (sa != sb) return sa > sb ? -1 : 1;
    if (a->depth != b->depth && a->depth != 0 && b->depth != 0) return a->depth < b->depth ? -1 : 1;
    if (a->y != b->y) return a->y < b->y ? -1 : 1;
    return a->x < b->x ? -1 : a->x > b->x ? 1 : 0;
}

int halo_track_frame(HaloTracker *tracker, const HaloCand *cands, int n, int quota, HaloDraw *out, int max)
{
    HaloCand sorted[1024];
    if (n > 1024) n = 1024;
    if (n > 0) memcpy(sorted, cands, sizeof sorted[0] * (size_t)n);
    qsort(sorted, (size_t)n, sizeof sorted[0], by_priority);
    unsigned char used[1024] = { 0 };
    for (int i = 0; i < HALO_TRACK_MAX; i++) {   /* the halos of the frame before take the nearest blob of their colour */
        HaloTrack *t = &tracker->t[i];
        if (!t->live) continue;
        t->seen = 0;
        const float px = t->x + t->vx, py = t->y + t->vy;
        int best = -1;
        float best_d = 0;
        const float reach = (float)MATCH_BASE + t->radius / MATCH_RADIUS_DIV;
        for (int k = 0; k < n; k++) {
            if (used[k] || sorted[k].value != t->value) continue;
            const float dx = (float)sorted[k].cx - px, dy = (float)sorted[k].cy - py;
            const float d = dx * dx + dy * dy;
            if (dx > reach || dx < -reach || dy > reach || dy < -reach) continue;
            if (best < 0 || d < best_d) best = k, best_d = d;
        }
        if (best < 0) {   /* lost: it drifts on as it was moving and fades */
            t->x += t->vx, t->y += t->vy;
            t->alpha -= (float)HALO_FADE_OUT_PERCENT / 100.0f;
            if (t->alpha <= 0.0f) t->live = 0;
            continue;
        }
        used[best] = 1;
        t->seen = 1;
        const HaloCand *c = &sorted[best];
        t->vx = (float)c->cx - t->x, t->vy = (float)c->cy - t->y;
        t->x = (float)c->cx, t->y = (float)c->cy;
        t->radius += ((float)c->radius - t->radius) * SMOOTH;
        t->area = c->area, t->depth = c->depth;
        t->area_avg += ((float)c->area - t->area_avg) * AREA_SMOOTH;
        t->alpha += (float)HALO_FADE_IN_PERCENT / 100.0f;
        if (t->alpha > 1.0f) t->alpha = 1.0f;
    }
    for (int k = 0; k < n; k++) {   /* a blob nobody had: a new halo, fading in, if there is room */
        if (used[k]) continue;
        for (int i = 0; i < HALO_TRACK_MAX; i++) {
            HaloTrack *t = &tracker->t[i];
            if (t->live) continue;
            *t = (HaloTrack){ (float)sorted[k].cx, (float)sorted[k].cy, 0, 0, (float)sorted[k].radius, (float)HALO_FADE_IN_PERCENT / 100.0f, 0,
                              sorted[k].value, sorted[k].area, sorted[k].depth, 1, 0, 1 };
            t->area_avg = (float)sorted[k].area;
            break;
        }
    }
    HaloTrack *order[HALO_TRACK_MAX];
    int live = 0;
    for (int i = 0; i < HALO_TRACK_MAX; i++)
        if (tracker->t[i].live) order[live++] = &tracker->t[i];
    qsort(order, (size_t)live, sizeof order[0], by_score);
    int count = 0, extra = 0, in_quota = 0;
    for (int k = 0; k < live; k++) {
        HaloTrack *t = order[k];
        int emit = 1;
        if (!t->seen) {   /* its blob was not found: it fades, over the quota */
            if (extra >= HALO_EXTRA) emit = 0;
            else extra++;
        } else if (in_quota < quota) {
            in_quota++;
            t->shown = 1;
        } else if (t->shown) {   /* pushed out of the quota: it fades rather than blinks out */
            t->shown = 0;
            t->alpha -= (float)HALO_FADE_OUT_PERCENT / 100.0f;
            if (t->alpha <= 0.0f) { t->live = 0; continue; }
            if (extra >= HALO_EXTRA) emit = 0;
            else extra++;
        } else {   /* waits its turn, not shown */
            if (t->alpha > (float)HALO_FADE_IN_PERCENT / 100.0f) t->alpha = (float)HALO_FADE_IN_PERCENT / 100.0f;
            emit = 0;
        }
        const int percent = (int)(t->alpha * 100.0f + 0.5f);
        if (!emit || percent < 1 || count >= max) continue;
        out[count++] = (HaloDraw){ (int)(t->x + 0.5f), (int)(t->y + 0.5f), (int)(t->radius + 0.5f), percent > 100 ? 100 : percent, t->value, (int)(t->area_avg * 16.0f + 0.5f) };
    }
    return count;
}
