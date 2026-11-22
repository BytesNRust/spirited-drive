/* See original_track_trace.h. */
#include "original_track_trace.h"

#include "formats/tri.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ANGLE_TURN 16384.0

static double angle_degrees(int16_t a)
{
    int v = a;
    if (v > 0x2000) v -= 0x3fff;   /* tri.h: above 0x2000 is negative via -0x3fff */
    return (double)v / ANGLE_TURN * 360.0;
}

static int fail(char *why, size_t cap, const char *text)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s", text);
    return 0;
}

int original_track_trace(const uint8_t *data, size_t size, int stride, Trace *out, char *why, size_t why_cap)
{
    Tri tri;
    if (stride < 1 || stride > 8) return fail(why, why_cap, "the trace's stride is 1..8");
    if (tri_open(&tri, data, size) != TRI_OK) return fail(why, why_cap, "the track's file does not open");
    const uint32_t last = tri.node_count;   /* the closing node's index */
    const int count = (int)(last / (uint32_t)stride) + 1 + ((last % (uint32_t)stride) != 0);
    if (count < TRACE_POINTS_MIN || count > PROJECT_POINTS_MAX) return fail(why, why_cap, "the track has too many or too few nodes for a trace");
    out->points = calloc((size_t)count, sizeof *out->points);
    out->keys = calloc((size_t)count * 2, sizeof *out->keys);
    out->count = 0, out->key_count = 0, out->key_cap = count * 2;
    if (out->points == NULL || out->keys == NULL) return free(out->points), free(out->keys), out->points = NULL, out->keys = NULL, fail(why, why_cap, "out of memory");
    TriNode first;
    tri_node(&tri, 0, &first);
    double width_prev = -1, bank_prev = 0;
    int have_bank = 0;
    for (uint32_t i = 0;; i += (uint32_t)stride) {
        if (i > last) i = last;
        TriNode n;
        if (tri_node(&tri, i, &n) != TRI_OK) return free(out->points), free(out->keys), out->points = NULL, out->keys = NULL, fail(why, why_cap, "a node does not read");
        TracePoint *p = &out->points[out->count];
        p->x = n.position.x / 65536.0, p->z = n.position.z / 65536.0;
        p->h = floor((n.position.y - first.position.y) / 65536.0 / TRACE_HEIGHT_STEP_M + 0.5) * TRACE_HEIGHT_STEP_M;
        const double width = fmin(fmax((n.left_verge + n.right_verge) / 2.0, PROJECT_WIDTH_MIN), PROJECT_WIDTH_MAX);
        const double bank = angle_degrees(n.slant_a);
        TraceKey k = {0};
        k.point = out->count;
        if (out->count == 0) out->width = width;
        else if (fabs(width - width_prev) >= 1.5) k.has |= TRACE_KEY_WIDTH, k.width_m = project_road_metres(width);
        if (fabs(bank - bank_prev) >= 0.75 || (out->count == 0 && fabs(bank) >= 0.75)) {
            k.has |= TRACE_KEY_BANK, k.bank_deg = fabs(bank) < 0.75 ? 0 : bank;
            have_bank = 1;
        }
        if (k.has != 0) {
            if (k.has & TRACE_KEY_WIDTH) width_prev = width;
            if (k.has & TRACE_KEY_BANK) bank_prev = k.bank_deg;
            out->keys[out->key_count++] = k;
        }
        if (out->count == 0) width_prev = width;
        ++out->count;
        if (i == last) break;
    }
    (void)have_bank;
    return 1;
}

int original_track_trace_error(const uint8_t *data, size_t size, const Trace *trace, double *max_m, double *mean_m, double *max_dy, char *why, size_t why_cap)
{
    Tri tri;
    TrackGenNode *nodes = NULL;
    int count = 0;
    ExchangeError err = {{0}};
    if (tri_open(&tri, data, size) != TRI_OK) return fail(why, why_cap, "the track's file does not open");
    if (!track_trace_nodes_keyed(trace->points, trace->count, trace->width, trace->keys, trace->key_count, &nodes, &count, &err)) return fail(why, why_cap, err.message);
    TriNode first;
    tri_node(&tri, 0, &first);
    double worst = 0, sum = 0, dy = 0;
    int n = 0;
    for (uint32_t i = 0; i <= tri.node_count; ++i, ++n) {   /* each of the TRI's nodes against the built road: the nearest of its segments on the plan */
        TriNode t;
        tri_node(&tri, i, &t);
        const double tx = t.position.x / 65536.0, tz = t.position.z / 65536.0;
        double best = 1e30, best_dy = 0;
        for (int j = 0; j + 1 < count; ++j) {
            const double ax = nodes[j].position[0] / 65536.0, az = nodes[j].position[2] / 65536.0;
            const double bx = nodes[j + 1].position[0] / 65536.0, bz = nodes[j + 1].position[2] / 65536.0;
            const double ex = bx - ax, ez = bz - az, len2 = ex * ex + ez * ez;
            double u = len2 > 0 ? ((tx - ax) * ex + (tz - az) * ez) / len2 : 0;
            u = fmin(fmax(u, 0), 1);
            const double px = ax + ex * u - tx, pz = az + ez * u - tz, d = sqrt(px * px + pz * pz);
            if (d < best) {
                const double y = (nodes[j].position[1] + (nodes[j + 1].position[1] - (double)nodes[j].position[1]) * u) / 65536.0;
                best = d, best_dy = fabs(y - (double)(t.position.y - first.position.y) / 65536.0);
            }
        }
        worst = fmax(worst, best), sum += best, dy = fmax(dy, best_dy);
    }
    free(nodes);
    *max_m = worst, *mean_m = n > 0 ? sum / n : 0, *max_dy = dy;
    return 1;
}

static uint8_t *read_tri(const char *cd, const char *stem, size_t *size, char *why, size_t cap)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/SIMDATA/MISC/%s.TRI", cd, stem);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return fail(why, cap, "the game's files have no such track (is the game's folder set?)"), NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d == NULL || fread(d, 1, (size_t)n, f) != (size_t)n) {
        free(d), fclose(f);
        return fail(why, cap, "the track's file does not read"), NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return d;
}

int original_track_exportable(const char *cd, const char *stem, char *why, size_t why_cap)
{
    size_t size;
    Tri tri;
    uint8_t *d = read_tri(cd, stem, &size, why, why_cap);
    if (d == NULL) return 0;
    int ok = tri_open(&tri, d, size) == TRI_OK;
    Trace trace = {0};
    TrackGenNode *nodes = NULL;
    int count = 0;
    ExchangeError err = {{0}};
    if (!ok) fail(why, why_cap, "the track's file does not open");
    else if (!(ok = original_track_trace(d, size, 4, &trace, why, why_cap))) {}
    else if (!(ok = track_trace_nodes_keyed(trace.points, trace.count, trace.width, trace.keys, trace.key_count, &nodes, &count, &err)))   /* the editor's own limits: 599 segments of the circuits' spacing */
        snprintf(why, why_cap, "%s: %s", stem, err.message);
    free(nodes), free(trace.points), free(trace.keys);
    free(d);
    return ok;
}

static void remove_tree(const char *dir)
{
    char cmd_path[1100];
    snprintf(cmd_path, sizeof cmd_path, "%s/trace.json", dir), remove(cmd_path);
    snprintf(cmd_path, sizeof cmd_path, "%s/project.ini", dir), remove(cmd_path);
    snprintf(cmd_path, sizeof cmd_path, "%s/GENERIC.FAM", dir), remove(cmd_path);
    rmdir(dir);
}

/* how many nodes of the road `trace` builds break a red limit (the ones Build refuses) */
static int red_nodes(const Trace *trace)
{
    TrackGenNode *nodes = NULL;
    int count = 0, red = 0;
    ExchangeError err = {{0}};
    if (!track_trace_nodes_keyed(trace->points, trace->count, trace->width, trace->keys, trace->key_count, &nodes, &count, &err)) return 1 << 20;
    TraceFlags *flags = calloc((size_t)count, sizeof *flags);
    if (flags != NULL) {
        track_trace_flags(nodes, count, flags);
        for (int i = 0; i < count; ++i) red += (flags[i] & TRACE_FLAG_RED_BITS) != 0;
    }
    free(flags), free(nodes);
    return red;
}

int original_track_export(const char *cd, const char *stem, const char *tracks_dir, const char *id, const char *name, const char *generic_art, char *dir, size_t dir_cap, char *why, size_t why_cap)
{
    char making[1100], path[1100];
    struct stat st;
    if (!mod_ini_id_ok(id)) return snprintf(why, why_cap, "the id may hold only a-z, 0-9 and '-' (not first), up to %d", MOD_INI_ID_MAX), 0;
    snprintf(path, sizeof path, "%s/%s", tracks_dir, id);
    if (stat(path, &st) == 0) return snprintf(why, why_cap, "\"%s\" already exists in the workspace's tracks: choose another name or id", id), 0;
    if (!original_track_exportable(cd, stem, why, why_cap)) return 0;
    size_t size;
    uint8_t *d = read_tri(cd, stem, &size, why, why_cap);
    if (d == NULL) return 0;
    Trace trace = {0};
    int made = 0;
    for (int stride = 4; stride >= 1; stride /= 2) {   /* the coarsest trace whose road has no red node: a steep drop of the CD's (TR7's 64 %) needs the points closer */
        if (made) free(trace.points), free(trace.keys), trace.points = NULL, trace.keys = NULL;
        made = original_track_trace(d, size, stride, &trace, why, why_cap);
        if (!made || red_nodes(&trace) == 0) break;
    }
    free(d);
    if (!made) return 0;
    snprintf(making, sizeof making, "%s.making", path);
    int ok = project_create(making, id, generic_art, why, why_cap) && project_save_trace(making, &trace, why, why_cap) && project_set_name(making, name != NULL && name[0] != '\0' ? name : id, why, why_cap);
    free(trace.points), free(trace.keys);
    if (ok && rename(making, path) != 0) ok = 0, snprintf(why, why_cap, "the folder %s could not be made", path);
    if (!ok) return remove_tree(making), 0;
    snprintf(dir, dir_cap, "%s", path);
    return 1;
}
