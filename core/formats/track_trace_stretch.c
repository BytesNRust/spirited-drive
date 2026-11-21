/* Colour by: see track_trace_stretch.h. */
#include "formats/track_trace_stretch.h"

#include "formats/track_template.h"
#include "formats/track_trace.h"

#include <math.h>

enum { CURVE_WINDOW = 4, CURVE_MIN_RUN = 8 };   /* nodes either side the curve is read over; a curve or straight shorter than this is its neighbour's (a spline's wobble) */

static const double WIDTH_EDGES_M[TRACE_WIDTH_BANDS - 1] = { 8, 11, 13, 16, 20 };   /* a road narrower than the first is band 0 */
static const double GRADE_EDGES_PCT[2] = { 8, 20 };                                  /* the grade steps after the level one (TRACE_GRADE_LEVEL_PCT) */
static const double EDGE_UNIT_M = 0.125;                                              /* the half width's byte is 1/8 m a unit */

static const char *const CURVE_NAMES[TRACE_CURVE_CLASSES] = { "straight", "left", "right" };
static const char *const BANK_NAMES[TRACE_BANK_CLASSES] = { "flat", "banked a little one way", "banked some one way", "banked a lot one way",
                                                            "banked a little the other way", "banked some the other way", "banked a lot the other way" };
static const char *const GRADE_NAMES[TRACE_GRADE_CLASSES] = { "level", "uphill a little", "uphill", "uphill steeply", "downhill a little", "downhill", "downhill steeply" };
static const char *const WIDTH_NAMES[TRACE_WIDTH_BANDS] = { "under 8 m", "8 to 11 m", "11 to 13 m", "13 to 16 m", "16 to 20 m", "20 m or more" };
static const char *const LANES_NAMES[3] = { "one lane each way", "one lane out, two back", "two lanes out, one back" };
static const char *const SHOULDER_NAMES[4] = { "verge both sides", "tarmac on the left", "tarmac on the right", "tarmac both sides" };
static const char *const FENCE_NAMES[TRACE_FENCE_CLASSES] = { "the template's fence", "low planks", "tall planks", "low rail", "tall rail", "low parapet", "tall parapet" };

int track_trace_class_count(TraceColourBy mode)
{
    switch (mode) {
    case TRACE_COLOUR_CURVE: return TRACE_CURVE_CLASSES;
    case TRACE_COLOUR_BANK: return TRACE_BANK_CLASSES;
    case TRACE_COLOUR_WIDTH: return TRACE_WIDTH_BANDS;
    case TRACE_COLOUR_LANES: return 3;
    case TRACE_COLOUR_SHOULDER: return 4;
    case TRACE_COLOUR_FENCE: return TRACE_FENCE_CLASSES;
    case TRACE_COLOUR_GRADE: return TRACE_GRADE_CLASSES;
    default: return 1;
    }
}

const char *track_trace_class_name(TraceColourBy mode, int klass)
{
    if (klass < 0 || klass >= track_trace_class_count(mode)) return "";
    switch (mode) {
    case TRACE_COLOUR_CURVE: return CURVE_NAMES[klass];
    case TRACE_COLOUR_BANK: return BANK_NAMES[klass];
    case TRACE_COLOUR_WIDTH: return WIDTH_NAMES[klass];
    case TRACE_COLOUR_LANES: return LANES_NAMES[klass];
    case TRACE_COLOUR_SHOULDER: return SHOULDER_NAMES[klass];
    case TRACE_COLOUR_FENCE: return FENCE_NAMES[klass];
    case TRACE_COLOUR_GRADE: return GRADE_NAMES[klass];
    default: return "";
    }
}

static int curve_class(const TrackGenNode *nodes, int count, int k)
{
    const int from = k - CURVE_WINDOW < 0 ? 0 : k - CURVE_WINDOW, to = k + CURVE_WINDOW > count - 1 ? count - 1 : k + CURVE_WINDOW;
    if (to <= from) return TRACE_CURVE_STRAIGHT;
    double metres = 0;
    for (int i = from + 1; i <= to; ++i) metres += track_trace_step(nodes, i);
    const double degrees = track_trace_turn_degrees(nodes[from].heading, nodes[to].heading);
    const double radians = fabs(degrees) * 3.14159265358979323846 / 180.0;
    if (radians <= 0 || metres / radians >= TRACE_STRAIGHT_RADIUS_M) return TRACE_CURVE_STRAIGHT;
    return degrees < 0 ? TRACE_CURVE_LEFT : TRACE_CURVE_RIGHT;
}

static int bank_class(double bank)
{
    const double size = fabs(bank);
    if (size < TRACE_BANK_FLAT_DEG) return 0;
    int step = (int)(size / TRACE_BANK_STEP_DEG);
    if (step > 2) step = 2;
    return 1 + step + (bank < 0 ? 3 : 0);
}

static int grade_class(double grade)
{
    const double size = fabs(grade);
    if (size < TRACE_GRADE_LEVEL_PCT) return 0;
    int step = 0;
    while (step < 2 && size >= GRADE_EDGES_PCT[step]) ++step;
    return 1 + step + (grade < 0 ? 3 : 0);
}

static int width_class(const TrackGenNode *n)
{
    const double metres = (n->width + (n->width_r > 0 ? n->width_r : n->width)) * EDGE_UNIT_M;
    int band = 0;
    while (band < TRACE_WIDTH_BANDS - 1 && metres >= WIDTH_EDGES_M[band]) ++band;
    return band;
}

static int tarmac(const TrackGenNode *n, int side)
{
    return (n->side[side].has & TRACK_GEN_SIDE_TARMAC) && n->side[side].tarmac;
}

static int fence_class(const TrackGenFence *f)
{
    if (f->has == 0) return 0;
    const int art = (f->has & TRACK_GEN_FENCE_SLOT) ? f->slot / TRACK_GENERIC_FENCE_PICTURE_STEP : TRACE_FENCE_PLANKS;
    const int tall = (f->has & TRACK_GEN_FENCE_HEIGHT) && f->tall ? 1 : 0;
    return 1 + 2 * (art >= TRACE_FENCE_PICTURES ? TRACE_FENCE_PICTURES - 1 : art) + tall;
}

/* a run of fewer than CURVE_MIN_RUN nodes takes the class of the run before it (the first, of the one after): the shortest first, until none is left */
static void absorb_short_runs(uint8_t *c, int count)
{
    for (int pass = 0; pass < count; ++pass) {
        int a = 0, shortest = -1, shortest_first = 0, shortest_last = 0;
        while (a < count) {
            int b = a;
            while (b + 1 < count && c[b + 1] == c[a]) ++b;
            const int len = b - a + 1;
            if (len < CURVE_MIN_RUN && !(a == 0 && b == count - 1) && (shortest < 0 || len < shortest)) shortest = len, shortest_first = a, shortest_last = b;
            a = b + 1;
        }
        if (shortest < 0) return;
        const uint8_t into = shortest_first > 0 ? c[shortest_first - 1] : c[shortest_last + 1];
        for (int k = shortest_first; k <= shortest_last; ++k) c[k] = into;
    }
}

void track_trace_node_classes(const TrackGenNode *nodes, int count, TraceColourBy mode, uint8_t *out)
{
    for (int k = 0; k < count; ++k) {
        int c = 0;
        switch (mode) {
        case TRACE_COLOUR_CURVE: c = curve_class(nodes, count, k); break;
        case TRACE_COLOUR_BANK: c = bank_class(nodes[k].bank); break;
        case TRACE_COLOUR_WIDTH: c = width_class(&nodes[k]); break;
        case TRACE_COLOUR_LANES: c = nodes[k].lanes == TRACK_GEN_LANES_1_2 ? 1 : nodes[k].lanes == TRACK_GEN_LANES_2_1 ? 2 : 0; break;
        case TRACE_COLOUR_SHOULDER: c = tarmac(&nodes[k], TRACK_GEN_LEFT) | tarmac(&nodes[k], TRACK_GEN_RIGHT) << 1; break;
        case TRACE_COLOUR_FENCE: c = fence_class(&nodes[k].fence); break;
        case TRACE_COLOUR_GRADE: c = grade_class(nodes[k].grade); break;
        default: break;
        }
        out[k] = (uint8_t)c;
    }
    if (mode == TRACE_COLOUR_CURVE) absorb_short_runs(out, count);
}

int track_trace_stretches(const uint8_t *classes, int count, TraceStretch *out, int cap)
{
    int runs = 0;
    for (int k = 0; classes != NULL && k < count; ++k) {
        if (k > 0 && classes[k] == classes[k - 1]) {
            if (out != NULL && runs <= cap) out[runs - 1].last = k;
            continue;
        }
        if (out != NULL && runs < cap) out[runs].first = k, out[runs].last = k, out[runs].klass = classes[k];
        ++runs;
    }
    return runs;
}

int track_trace_stretch_of(const uint8_t *classes, int count, int node, int *first, int *last)
{
    if (classes == NULL || node < 0 || node >= count) return 0;
    int a = node, b = node;
    while (a > 0 && classes[a - 1] == classes[node]) --a;
    while (b < count - 1 && classes[b + 1] == classes[node]) ++b;
    *first = a, *last = b;
    return 1;
}
