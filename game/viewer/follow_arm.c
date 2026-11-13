/* The road behind the camera's node (follow_arm.h). */
#include "game/viewer/follow_arm.h"

#include "game/gfx/x86_int.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_tables.h"
#include "game/sys/fixed_math.h"
#include "game/race/view.h"

#include <string.h>

enum { SPARE_RECORDS = 2 };

/* port, the classic sink: a corner nearer than `plane` is slid along its quad's edge to the plane, towards a corner in front
 * (an adjacent one, else the opposite; two adjacent ones are averaged), so a quad across the camera still projects as a
 * quad (its texture is squeezed into what is left of it). Returns 0 when no corner is in front. */
int follow_arm_clip_quad(const int32_t *const in[4], int32_t plane, int32_t out[4][3])
{
    int front = 0;
    for (int c = 0; c < 4; c++) front += in[c][2] >= plane;
    if (front == 0) return 0;
    for (int c = 0; c < 4; c++) {
        if (in[c][2] >= plane) {
            memcpy(out[c], in[c], sizeof out[c]);
            continue;
        }
        const int towards[2] = { (c + 3) & 3, (c + 1) & 3 };
        int32_t sum[3] = { 0, 0, 0 };
        int used = 0;
        for (int n = 0; n < 2; n++) {
            const int32_t *to = in[towards[n]];
            if (to[2] < plane) continue;
            const int64_t span = (int64_t)to[2] - in[c][2], t = (int64_t)plane - in[c][2];
            for (int a = 0; a < 3; a++) sum[a] += (int32_t)(in[c][a] + (to[a] - in[c][a]) * t / span);
            used++;
        }
        if (used == 0) {
            const int32_t *to = in[(c + 2) & 3];
            const int64_t span = (int64_t)to[2] - in[c][2], t = (int64_t)plane - in[c][2];
            for (int a = 0; a < 3; a++) sum[a] = (int32_t)(in[c][a] + (to[a] - in[c][a]) * t / span);
            used = 1;
        }
        for (int a = 0; a < 3; a++) out[c][a] = sum[a] / used;
    }
    return 1;
}

int32_t follow_arm_kept_behind(const TrackCacheWindow *w, int32_t wanted)
{
    const int32_t kept = w->behind - w->ahead;
    if (kept <= 0 || wanted <= 0) return 0;
    return wanted < kept ? wanted : kept;
}

int32_t follow_arm_segments(void)
{
    return track_cache_records() > TRACK_CACHE_RECORDS ? FOLLOW_ARM_RING_SEGMENTS : FOLLOW_ARM_SEGMENTS;
}

void follow_arm_window(TrackRender *r, int32_t segments)
{
    /* the segments loaded when the window starts again: the exe's five behind, or `segments` when they fit the ring's records (62 of its 64) */
    const int32_t records = track_cache_records() - SPARE_RECORDS;
    int32_t behind = segments > 5 ? segments : 5;
    if (r->window.ahead + behind > records) behind = records - r->window.ahead;
    if (behind > 5) r->window.behind = r->window.ahead + behind;
}
