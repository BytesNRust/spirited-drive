#define _POSIX_C_SOURCE 200809L  /* strtok_r, which the web build's C11 headers keep behind it */
#include "formats/track_gen.h"

#include "formats/track_frame.h"
#include "formats/tri.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    NODES_OFFSET = 0x98C,
    NODE_SIZE = 36,
    BRIGHTNESS_OFFSET = 0x15B0C,
    BRIGHTNESS_SIZE = 3,
    ROW_BASE = 0x18,         /* first terrain row inside a TRKD chunk (tri_exchange segment_fields) */
    ROW_SIZE = 66,
    ROW_RIGHT0 = 6,          /* right[0].x inside a row */
    ROW_LEFT0 = 0x24,        /* left[0].x inside a row */
    ANGLE_TURN = 0x4000,     /* 14-bit angles: one full turn */
    VECTOR_ONE = 32767,
    TEMPLATE_SEGMENT = 10,   /* the cross-section is taken from this strip of the template (or the last one) */
    TEMPLATE_BRIGHTNESS = 10,
    WIDTH_MIN = 8,
    WIDTH_MAX = 120,
    MAX_SECTIONS = 256
};

#define MAX_GRADE_PERCENT 30.0
#define MAX_BANK_DEGREES 20.0
#define MAX_TURN_DEGREES_PER_NODE TRACK_GEN_LAYOUT_TURN_MAX_DEGREES

typedef struct {
    int segments;
    double turn, grade, bank, width;   /* turn is the section's total; the others are the values at its end */
} Section;

static void put_i16(uint8_t *p, int v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_i32(uint8_t *p, int32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)((uint32_t)v >> (8 * i)); }
static int get_i16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }

static int parse_number(const char *text, double *value)
{
    char *end = NULL;
    *value = strtod(text, &end);
    return end != text && *end == '\0' && isfinite(*value);
}

static int in_range(ExchangeError *err, int line, const char *key, double v, double lo, double hi)
{
    if (v >= lo && v <= hi) return 1;
    exchange_fail(err, "line %d: %s=%g is outside %g..%g", line, key, v, lo, hi);
    return 0;
}

/* One directive. `prev` holds the values the section starts from and is kept for the keys it does not name. */
static int parse_section(char *args, int line, const Section *prev, Section *out, ExchangeError *err)
{
    char *save = NULL;
    char *token = strtok_r(args, " \t\r", &save);
    double count;
    if (token == NULL || !parse_number(token, &count) || count != floor(count)) {
        exchange_fail(err, "line %d: section needs a whole number of segments", line);
        return 0;
    }
    if (count < 1 || count > TRACK_GEN_MAX_SEGMENTS) {
        exchange_fail(err, "line %d: a section of %g segments is outside 1..%d", line, count, TRACK_GEN_MAX_SEGMENTS);
        return 0;
    }
    *out = *prev;
    out->segments = (int)count;
    out->turn = 0;
    while ((token = strtok_r(NULL, " \t\r", &save)) != NULL) {
        char *eq = strchr(token, '=');
        double v;
        if (eq == NULL || !parse_number(eq + 1, &v)) {
            exchange_fail(err, "line %d: '%s' is not key=number", line, token);
            return 0;
        }
        *eq = '\0';
        if (strcmp(token, "turn") == 0) {
            if (fabs(v) > MAX_TURN_DEGREES_PER_NODE * 4 * out->segments) {
                exchange_fail(err, "line %d: turn=%g over %d segments is more than %g degrees per node", line, v, out->segments,
                              MAX_TURN_DEGREES_PER_NODE);
                return 0;
            }
            out->turn = v;
        } else if (strcmp(token, "grade") == 0) {
            if (!in_range(err, line, token, v, -MAX_GRADE_PERCENT, MAX_GRADE_PERCENT)) return 0;
            out->grade = v;
        } else if (strcmp(token, "bank") == 0) {
            if (!in_range(err, line, token, v, -MAX_BANK_DEGREES, MAX_BANK_DEGREES)) return 0;
            out->bank = v;
        } else if (strcmp(token, "width") == 0) {
            if (!in_range(err, line, token, v, WIDTH_MIN, WIDTH_MAX)) return 0;
            out->width = v;
        } else {
            exchange_fail(err, "line %d: unknown key '%s' (turn, grade, bank, width)", line, token);
            return 0;
        }
    }
    return 1;
}

static int parse_layout(const char *layout, size_t len, double start_width, Section *sections, int *count, int *segments,
                        ExchangeError *err)
{
    char *text = malloc(len + 1);
    if (text == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(text, layout, len);
    text[len] = '\0';
    Section prev = { 0, 0, 0, 0, start_width };
    int ok = 1;
    *count = 0;
    *segments = 0;
    int line = 0;
    for (char *row = text; row != NULL && ok;) {
        char *next = strchr(row, '\n');
        if (next != NULL) *next++ = '\0';
        ++line;
        char *hash = strchr(row, '#');
        if (hash != NULL) *hash = '\0';
        while (*row == ' ' || *row == '\t' || *row == '\r') ++row;
        const size_t word = strcspn(row, " \t\r");
        if (*row == '\0') {
            /* blank or comment */
        } else if (word != 7 || strncmp(row, "section", 7) != 0) {
            row[word] = '\0';
            exchange_fail(err, "line %d: unknown directive '%s' (only 'section')", line, row);
            ok = 0;
        } else if (*count == MAX_SECTIONS) {
            exchange_fail(err, "line %d: more than %d sections", line, MAX_SECTIONS);
            ok = 0;
        } else if (parse_section(row + word, line, &prev, &sections[*count], err)) {
            prev = sections[*count];
            *segments += prev.segments;
            ++*count;
        } else {
            ok = 0;
        }
        row = next;
    }
    if (ok && *count == 0) { exchange_fail(err, "the layout has no section"); ok = 0; }
    if (ok && (*segments < TRACK_GEN_MIN_SEGMENTS || *segments > TRACK_GEN_MAX_SEGMENTS)) {
        exchange_fail(err, "the layout has %d segments in all, the game takes %d..%d", *segments, TRACK_GEN_MIN_SEGMENTS,
                      TRACK_GEN_MAX_SEGMENTS);
        ok = 0;
    }
    free(text);
    return ok;
}

/* Node i of the track: heading accumulates the turn of the sections, the other values ramp inside a section. */
static void node_shapes(const Section *sections, int count, double start_width, TrackGenNode *shapes)
{
    int node = 0;
    TrackGenNode at = { 0, 0, 0, start_width, { 0, 0, 0 }, { { 0, 0 }, { 0, 0 } } };
    for (int s = 0; s < count; ++s) {
        const int nodes = sections[s].segments * TRI_NODES_PER_SEGMENT;
        const TrackGenNode from = at;
        for (int k = 0; k < nodes; ++k, ++node) {
            const double t = (double)k / nodes;
            shapes[node].heading = from.heading + sections[s].turn / 360.0 * ANGLE_TURN * t;
            shapes[node].grade = from.grade + (sections[s].grade - from.grade) * t;
            shapes[node].bank = from.bank + (sections[s].bank - from.bank) * t;
            shapes[node].width = from.width + (sections[s].width - from.width) * t;
            memset(shapes[node].side, 0, sizeof shapes[node].side);   /* a layout has no sides of its own: the template's */
        }
        at.heading = from.heading + sections[s].turn / 360.0 * ANGLE_TURN;
        at.grade = sections[s].grade;
        at.bank = sections[s].bank;
        at.width = sections[s].width;
    }
    shapes[node] = at;   /* the closing node */
}

static unsigned angle14(double units) { return (unsigned)((long)lround(units) & (ANGLE_TURN - 1)); }

static int clamp_byte(double v) { return v < 0 ? 0 : v > 255 ? 255 : (int)lround(v); }

static void write_node(uint8_t *dst, const uint8_t *model, const TrackGenNode *shape, const int32_t pos[3], const TriNode *tmpl)
{
    memcpy(dst, model, NODE_SIZE);
    const int width = (int)lround(shape->width);
    dst[0] = (uint8_t)clamp_byte(width);
    dst[1] = (uint8_t)clamp_byte(width + (tmpl->right_verge - tmpl->left_verge));
    dst[2] = (uint8_t)clamp_byte(width + (tmpl->left_barrier - tmpl->left_verge));
    dst[3] = (uint8_t)clamp_byte(width + (tmpl->right_barrier - tmpl->left_verge));
    put_i32(dst + 8, pos[0]);
    put_i32(dst + 12, pos[1]);
    put_i32(dst + 16, pos[2]);
    const double heading = shape->heading / ANGLE_TURN * 2 * M_PI;
    const double bank = shape->bank * M_PI / 180;
    put_u16(dst + 0x14, angle14(atan(shape->grade / 100.0) / (2 * M_PI) * ANGLE_TURN));
    put_u16(dst + 0x16, angle14(shape->bank / 360.0 * ANGLE_TURN));
    put_u16(dst + 0x18, angle14(shape->heading));
    put_u16(dst + 0x1a, 0);
    put_i16(dst + 0x1c, (int)lround(cos(heading) * cos(bank) * VECTOR_ONE));
    put_i16(dst + 0x1e, -(int)lround(sin(bank) * VECTOR_ONE));
    put_i16(dst + 0x20, -(int)lround(sin(heading) * cos(bank) * VECTOR_ONE));
    put_u16(dst + 0x22, 0);
    for (int side = TRACK_GEN_LEFT; side <= TRACK_GEN_RIGHT; ++side)   /* a tarmac shoulder: that side's nibble of flags[2] (high left, low right) is row 0 */
        if ((shape->side[side].has & TRACK_GEN_SIDE_TARMAC) && shape->side[side].tarmac) dst[6] &= (uint8_t)(side == TRACK_GEN_LEFT ? 0x0f : 0xf0);
}

static void write_strip(uint8_t *seg, const uint8_t *model, const TrackGenNode *shapes, const TriNode *tmpl)
{
    memcpy(seg, model, TRI_SEGMENT_SIZE);
    for (int row = 0; row < TRI_NODES_PER_SEGMENT; ++row) {
        uint8_t *r = seg + ROW_BASE + ROW_SIZE * row;
        const int width = (int)lround(shapes[row].width);
        put_i16(r + ROW_RIGHT0, get_i16(r + ROW_RIGHT0) + 16 * (width - tmpl->left_verge));
        put_i16(r + ROW_LEFT0, get_i16(r + ROW_LEFT0) - 16 * (width - tmpl->left_verge));
    }
}

/* the frame of a node as the file will hold it: the same angles write_node stores */
static void shape_frame(const TrackGenNode *shape, TrackFrame *frame)
{
    track_frame_make((int32_t)angle14(shape->heading), (int32_t)angle14(atan(shape->grade / 100.0) / (2 * M_PI) * ANGLE_TURN),
                     (int32_t)angle14(shape->bank / 360.0 * ANGLE_TURN), frame);
}

/* one point of a row (3 x i16 deltas in world axes) from the template node's axes to the new node's, `lateral` added across the road on the
 * way. `*across` is the point's lateral reach from the road's centre in the new axes, summed along the row's side (`*across` of the side's
 * point before, in and out): when it passes `cap` the point is held at `cap`, the lateral step of the chain shortened to match. */
static void turn_point(uint8_t *p, const TrackFrame *from, const TrackFrame *to, int lateral, int hold, double cap, double *across)
{
    const int32_t world[3] = { get_i16(p), get_i16(p + 2), get_i16(p + 4) };
    int32_t local[3], turned[3];
    track_frame_to_local(from, world, local);
    local[0] += lateral;
    if (hold) {
        const double reach = *across + local[0];
        double held = fabs(reach) > cap ? (reach < 0 ? -cap : cap) : reach;
        if (fabs(held) < fabs(*across)) held = *across;   /* never back towards the road: the points before it stay where they are */
        local[0] = (int32_t)lround(held - *across);
        *across = held;
    } else {
        *across += local[0];
    }
    track_frame_to_world(to, local, turned);
    for (int k = 0; k < 3; ++k) put_i16(p + 2 * k, turned[k] < -32768 ? -32768 : turned[k] > 32767 ? 32767 : turned[k]);
}

/* How far across the road, in the TRI's raw units (1/128 of a unit), the terrain of node i may reach on the inside of the curve: TRACK_GEN_TERRAIN_REACH
 * of the curve's radius there, the radius being a node length over the heading's turn into or out of the node (the larger). INFINITY on a straight.
 * `*inner` is that side's row side: 0 the right points (a right turn, the heading growing), 1 the left. */
static double terrain_cap(const TrackGenNode *shapes, int i, int count, int *inner)
{
    double turn = 0, step = (double)TRACK_GEN_NODE_LENGTH / 65536.0;
    *inner = 0;
    for (int k = i > 0 ? i - 1 : i; k <= i && k + 1 < count; ++k) {
        double d = fmod(shapes[k + 1].heading - shapes[k].heading, ANGLE_TURN);
        if (d > ANGLE_TURN / 2) d -= ANGLE_TURN;
        if (d < -ANGLE_TURN / 2) d += ANGLE_TURN;
        if (fabs(d) > turn) {
            turn = fabs(d);
            *inner = d > 0 ? 0 : 1;
            const double dx = ((double)shapes[k + 1].position[0] - shapes[k].position[0]) / 65536.0;
            const double dz = ((double)shapes[k + 1].position[2] - shapes[k].position[2]) / 65536.0;
            step = hypot(dx, dz);   /* the real distance between the two nodes: explicit nodes may be 1..20 units apart */
        }
    }
    if (turn < 1.0) return INFINITY;   /* under a 16384th of a turn: a line, as far as a byte of the row can tell */
    return step / (turn / ANGLE_TURN * 2 * M_PI) * TRACK_GEN_TERRAIN_REACH * 128.0;
}

/* The terrain caps of all rows, [right, left]: each row's terrain_cap on its inner side, INFINITY on the other; then every side's cap eased along the
 * road so it changes by at most one node length (TRACK_GEN_NODE_LENGTH, in raw units) from one row to the next. Without it the edge of the terrain
 * jumps wherever a cap starts, ends or changes side (the entry of a curve, an S's inflection); with it the cap runs out over the rows before and
 * after, as a slope. Malloc'd, `count` pairs; NULL when out of memory. */
static double (*terrain_caps(const TrackGenNode *shapes, int count))[2]
{
    double (*caps)[2] = malloc(sizeof *caps * (size_t)(count > 0 ? count : 1));
    if (caps == NULL) return NULL;
    const double slope = (double)TRACK_GEN_NODE_LENGTH / 65536.0 * 128.0;
    for (int i = 0; i < count; ++i) {
        int inner = 0;
        const double cap = terrain_cap(shapes, i, count, &inner);
        caps[i][inner] = cap;
        caps[i][1 - inner] = INFINITY;
    }
    for (int side = 0; side < 2; ++side) {
        for (int i = 1; i < count; ++i) caps[i][side] = fmin(caps[i][side], caps[i - 1][side] + slope);
        for (int i = count - 2; i >= 0; --i) caps[i][side] = fmin(caps[i][side], caps[i + 1][side] + slope);
    }
    return caps;
}

/* write_strip with the rows turned: the template's, in its nodes' own axes (`from`, one frame a row), laid along the new nodes' (`shapes`).
 * `caps` are the rows' terrain_caps, [right, left]: the points from the third on of a side do not reach further across than its cap (the two nearest
 * the road, its edge and its verge, are the road's own and are kept). */
static void write_strip_turned(uint8_t *seg, const uint8_t *model, const TrackGenNode *shapes, const TriNode *tmpl, const TrackFrame from[4],
                               const double caps[4][2])
{
    memcpy(seg, model, TRI_SEGMENT_SIZE);
    for (int row = 0; row < TRI_NODES_PER_SEGMENT; ++row) {
        uint8_t *r = seg + ROW_BASE + ROW_SIZE * row;
        TrackFrame to;
        shape_frame(&shapes[row], &to);
        const int widen = 16 * ((int)lround(shapes[row].width) - tmpl->left_verge);
        double across[2] = { 0, 0 };   /* right, left */
        for (int point = 0; point < 11; ++point) {
            const int lateral = point == 1 ? widen : point == 6 ? -widen : 0;   /* right[0] and left[0]: the road's edges */
            const int side = point >= 6, on_side = point == 0 ? -1 : point - 1 - 5 * side;   /* 0..4 along its side; -1 the middle */
            const int hold = on_side >= 2 && caps[row][side] != INFINITY;
            double middle = 0;
            turn_point(r + 6 * point, &from[row], &to, lateral, hold, caps[row][side], on_side < 0 ? &middle : &across[side]);
        }
    }
}

/* the positions along the headings: each node one node length from the one before (the closing node's too, as the CD's) */
static void integrate_positions(TrackGenNode *nodes, int count)
{
    int32_t pos[3] = { 0, 0, 0 };
    for (int i = 0; i < count; ++i) {
        memcpy(nodes[i].position, pos, sizeof pos);
        const double heading = nodes[i].heading / ANGLE_TURN * 2 * M_PI;
        pos[0] += (int32_t)lround(TRACK_GEN_NODE_LENGTH * sin(heading));
        pos[1] += (int32_t)lround(TRACK_GEN_NODE_LENGTH * nodes[i].grade / 100.0);
        pos[2] += (int32_t)lround(TRACK_GEN_NODE_LENGTH * cos(heading));
    }
}

int track_layout_nodes(const char *layout, size_t layout_len, double start_width, TrackGenNode **nodes, int *count, ExchangeError *err)
{
    *nodes = NULL;
    Section sections[MAX_SECTIONS];
    int sections_count, segments;
    if (!parse_layout(layout, layout_len, start_width, sections, &sections_count, &segments, err)) return 0;
    const int n = segments * TRI_NODES_PER_SEGMENT + 1;
    TrackGenNode *made = malloc(sizeof *made * (size_t)n);
    if (made == NULL) { exchange_fail(err, "out of memory"); return 0; }
    node_shapes(sections, sections_count, start_width, made);
    integrate_positions(made, n);
    *nodes = made;
    *count = n;
    return 1;
}

enum { STEP_MIN = 1 << 16, STEP_MAX = 20 << 16, HEADING_MAX = 1 << 24 };   /* steps of 1 and 20 units in 16.16; a thousand turns */

/* a node out of range, or too far a turn or step from the one before: why, naming it; 1 when it is as the generator takes it */
static int check_node(const TrackGenNode *n, int i, int turned, ExchangeError *err)
{
    if (!isfinite(n[i].heading) || !isfinite(n[i].grade) || !isfinite(n[i].bank) || !isfinite(n[i].width))
        return exchange_fail(err, "node %d: a value is not a finite number", i), 0;
    if (fabs(n[i].heading) > HEADING_MAX)   /* 14-bit units: a thousand turns is no heading, and lround of more is undefined */
        return exchange_fail(err, "node %d: heading=%g is outside +-%d", i, n[i].heading, HEADING_MAX), 0;
    if (n[i].width < 0 || n[i].width > 255)   /* a byte: the CD's own templates start from 0 (CY1..3) to 123 (TR5), outside what a layout may ask for */
        return exchange_fail(err, "node %d: width=%g is outside 0..255", i, n[i].width), 0;
    if (fabs(n[i].grade) > MAX_GRADE_PERCENT) return exchange_fail(err, "node %d: grade=%g is outside +-%g", i, n[i].grade, MAX_GRADE_PERCENT), 0;
    if (fabs(n[i].bank) > MAX_BANK_DEGREES) return exchange_fail(err, "node %d: bank=%g is outside +-%g", i, n[i].bank, MAX_BANK_DEGREES), 0;
    for (int side = TRACK_GEN_LEFT; side <= TRACK_GEN_RIGHT; ++side)
        if (n[i].side[side].tarmac != 0 && n[i].side[side].tarmac != 1)
            return exchange_fail(err, "node %d: the %s side's tarmac=%d is not 0 or 1", i, side == TRACK_GEN_LEFT ? "left" : "right", n[i].side[side].tarmac), 0;
    if (i == 0) return 1;
    double turn = fmod(n[i].heading - n[i - 1].heading, ANGLE_TURN);
    if (turn > ANGLE_TURN / 2) turn -= ANGLE_TURN;
    if (turn < -ANGLE_TURN / 2) turn += ANGLE_TURN;
    const double degrees = turn * 360.0 / ANGLE_TURN;
    const double most = turned ? TRACK_GEN_TURN_MAX_DEGREES : MAX_TURN_DEGREES_PER_NODE;
    if (fabs(degrees) > most + 1e-6)
        return exchange_fail(err, "node %d: it turns %g degrees from the node before, the most is %g", i, degrees, most), 0;
    const double dx = (double)n[i].position[0] - n[i - 1].position[0], dy = (double)n[i].position[1] - n[i - 1].position[1];
    const double dz = (double)n[i].position[2] - n[i - 1].position[2], step = sqrt(dx * dx + dy * dy + dz * dz);
    if (step < STEP_MIN || step > STEP_MAX)
        return exchange_fail(err, "node %d: it is %g units from the node before, it must be 1..20", i, step / 65536), 0;
    return 1;
}

static int generate_nodes(const TrackGenNode *shapes, int count, const uint8_t *template_tri, size_t template_size, int turned, uint8_t **out,
                          size_t *out_len, ExchangeError *err)
{
    *out = NULL;
    if (shapes == NULL || count < 1 || (count - 1) % TRI_NODES_PER_SEGMENT != 0) {
        exchange_fail(err, "a track has 4 nodes a segment and a closing one, not %d nodes", count);
        return 0;
    }
    const int segments = (count - 1) / TRI_NODES_PER_SEGMENT, nodes = count - 1;
    if (segments < TRACK_GEN_MIN_SEGMENTS || segments > TRACK_GEN_MAX_SEGMENTS) {
        exchange_fail(err, "a track of %d segments is outside %d..%d", segments, TRACK_GEN_MIN_SEGMENTS, TRACK_GEN_MAX_SEGMENTS);
        return 0;
    }
    for (int i = 0; i < count; ++i)
        if (!check_node(shapes, i, turned, err)) return 0;
    Tri tri;
    TriParts parts;
    TriNode model;
    if (tri_open(&tri, template_tri, template_size) != TRI_OK || tri_parts(&tri, &parts) != TRI_OK ||
        tri.segment_count < 1 || tri_node(&tri, 1, &model) != TRI_OK) {
        exchange_fail(err, "the template is not a usable TRI");
        return 0;
    }
    uint8_t *header = calloc(1, TRI_HEADER_SIZE);
    uint8_t *scenery = malloc((size_t)TRI_SCENERY_MAX * TRI_SCENERY_SIZE);
    uint8_t *terrain = malloc((size_t)segments * TRI_SEGMENT_SIZE);
    if (header == NULL || scenery == NULL || terrain == NULL) {
        exchange_fail(err, "out of memory");
        free(header); free(scenery); free(terrain);
        return 0;
    }

    memcpy(header, template_tri, TRI_HEADER_SIZE);
    memset(header + NODES_OFFSET, 0, (size_t)NODE_SIZE * TRI_NODE_CAPACITY);
    memset(header + BRIGHTNESS_OFFSET, 0, (size_t)BRIGHTNESS_SIZE * TRI_SEGMENT_CAPACITY);
    put_u16(header + 4, 0);                               /* a point-to-point stage */
    const uint8_t *model_node = template_tri + NODES_OFFSET + NODE_SIZE;
    for (int i = 0; i <= nodes; ++i) write_node(header + NODES_OFFSET + NODE_SIZE * i, model_node, &shapes[i], shapes[i].position, &model);
    put_u16(header + 0x18, (unsigned)(header[NODES_OFFSET + 0x14] | header[NODES_OFFSET + 0x15] << 8));
    put_u16(header + 0x20, (unsigned)(header[NODES_OFFSET + 0x18] | header[NODES_OFFSET + 0x19] << 8));

    uint8_t brightness[BRIGHTNESS_SIZE];
    if (tri_brightness(&tri, tri.segment_count > TEMPLATE_BRIGHTNESS ? TEMPLATE_BRIGHTNESS : tri.segment_count - 1, brightness) != TRI_OK) {
        memset(brightness, 0, sizeof brightness);
    }
    const uint32_t model_segment = tri.segment_count > TEMPLATE_SEGMENT ? TEMPLATE_SEGMENT : tri.segment_count - 1;
    TrackFrame model_frames[TRI_NODES_PER_SEGMENT];   /* the axes of the template nodes the model strip's rows belong to (turned only) */
    for (uint32_t r = 0; turned && r < TRI_NODES_PER_SEGMENT; ++r) {
        TriNode at;
        if (tri_node(&tri, model_segment * TRI_NODES_PER_SEGMENT + r, &at) != TRI_OK) {
            exchange_fail(err, "the template is not a usable TRI");
            free(header); free(scenery); free(terrain);
            return 0;
        }
        track_frame_of_node(&at, &model_frames[r]);
    }
    double (*caps)[2] = turned ? terrain_caps(shapes, nodes + 1) : NULL;
    if (turned && caps == NULL) {
        exchange_fail(err, "out of memory");
        free(header); free(scenery); free(terrain);
        return 0;
    }
    for (int s = 0; s < segments; ++s) {
        memcpy(header + BRIGHTNESS_OFFSET + BRIGHTNESS_SIZE * s, brightness, sizeof brightness);
        if (turned) {
            write_strip_turned(terrain + (size_t)s * TRI_SEGMENT_SIZE, parts.terrain + (size_t)model_segment * TRI_SEGMENT_SIZE,
                               shapes + s * TRI_NODES_PER_SEGMENT, &model, model_frames, (const double (*)[2])caps[s * TRI_NODES_PER_SEGMENT]);
        }
        else
            write_strip(terrain + (size_t)s * TRI_SEGMENT_SIZE, parts.terrain + (size_t)model_segment * TRI_SEGMENT_SIZE,
                        shapes + s * TRI_NODES_PER_SEGMENT, &model);
    }
    free(caps);
    memcpy(scenery, parts.scenery, (size_t)TRI_SCENERY_MAX * TRI_SCENERY_SIZE);
    memset(scenery, 0xff, 4);                             /* the first record ends the list: no placements */

    const TriParts made = { header, parts.descriptors, parts.descriptor_count, scenery, terrain, (uint32_t)segments, NULL, 0 };
    const size_t size = tri_write_size(&made);
    uint8_t *file = malloc(size);
    int ok = file != NULL && tri_write(&made, file, size, out_len) == TRI_OK;
    if (!ok) {
        free(file);
        exchange_fail(err, "cannot assemble the TRI");
    } else {
        *out = file;
    }
    free(header); free(scenery); free(terrain);
    return ok;
}

int track_generate_nodes(const TrackGenNode *shapes, int count, const uint8_t *template_tri, size_t template_size, uint8_t **out,
                         size_t *out_len, ExchangeError *err)
{
    return generate_nodes(shapes, count, template_tri, template_size, 0, out, out_len, err);
}

int track_generate_nodes_turned(const TrackGenNode *shapes, int count, const uint8_t *template_tri, size_t template_size, uint8_t **out,
                                size_t *out_len, ExchangeError *err)
{
    return generate_nodes(shapes, count, template_tri, template_size, 1, out, out_len, err);
}

int track_generate(const char *layout, size_t layout_len, const uint8_t *template_tri, size_t template_size,
                   uint8_t **out, size_t *out_len, ExchangeError *err)
{
    *out = NULL;
    Tri tri;
    TriParts parts;
    TriNode model;
    if (tri_open(&tri, template_tri, template_size) != TRI_OK || tri_parts(&tri, &parts) != TRI_OK ||
        tri.segment_count < 1 || tri_node(&tri, 1, &model) != TRI_OK) {
        exchange_fail(err, "the template is not a usable TRI");
        return 0;
    }
    TrackGenNode *nodes;
    int count;
    if (!track_layout_nodes(layout, layout_len, model.left_verge, &nodes, &count, err)) return 0;
    const int ok = track_generate_nodes(nodes, count, template_tri, template_size, out, out_len, err);
    free(nodes);
    return ok;
}
