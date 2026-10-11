#include "formats/track_gen.h"

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
#define MAX_TURN_DEGREES_PER_NODE 6.0

typedef struct {
    int segments;
    double turn, grade, bank, width;   /* turn is the section's total; the others are the values at its end */
} Section;

typedef struct {
    double heading;   /* 14-bit units */
    double grade;     /* percent */
    double bank;      /* degrees */
    double width;
} NodeShape;

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
static void node_shapes(const Section *sections, int count, double start_width, NodeShape *shapes)
{
    int node = 0;
    NodeShape at = { 0, 0, 0, start_width };
    for (int s = 0; s < count; ++s) {
        const int nodes = sections[s].segments * TRI_NODES_PER_SEGMENT;
        const NodeShape from = at;
        for (int k = 0; k < nodes; ++k, ++node) {
            const double t = (double)k / nodes;
            shapes[node].heading = from.heading + sections[s].turn / 360.0 * ANGLE_TURN * t;
            shapes[node].grade = from.grade + (sections[s].grade - from.grade) * t;
            shapes[node].bank = from.bank + (sections[s].bank - from.bank) * t;
            shapes[node].width = from.width + (sections[s].width - from.width) * t;
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

static void write_node(uint8_t *dst, const uint8_t *model, const NodeShape *shape, const int32_t pos[3], const TriNode *tmpl)
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
}

static void write_strip(uint8_t *seg, const uint8_t *model, const NodeShape *shapes, const TriNode *tmpl)
{
    memcpy(seg, model, TRI_SEGMENT_SIZE);
    for (int row = 0; row < TRI_NODES_PER_SEGMENT; ++row) {
        uint8_t *r = seg + ROW_BASE + ROW_SIZE * row;
        const int width = (int)lround(shapes[row].width);
        put_i16(r + ROW_RIGHT0, get_i16(r + ROW_RIGHT0) + 16 * (width - tmpl->left_verge));
        put_i16(r + ROW_LEFT0, get_i16(r + ROW_LEFT0) - 16 * (width - tmpl->left_verge));
    }
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
    Section sections[MAX_SECTIONS];
    int count, segments;
    if (!parse_layout(layout, layout_len, model.left_verge, sections, &count, &segments, err)) return 0;

    const int nodes = segments * TRI_NODES_PER_SEGMENT;
    NodeShape *shapes = malloc(sizeof *shapes * (size_t)(nodes + 1));
    uint8_t *header = calloc(1, TRI_HEADER_SIZE);
    uint8_t *scenery = malloc((size_t)TRI_SCENERY_MAX * TRI_SCENERY_SIZE);
    uint8_t *terrain = malloc((size_t)segments * TRI_SEGMENT_SIZE);
    if (shapes == NULL || header == NULL || scenery == NULL || terrain == NULL) {
        exchange_fail(err, "out of memory");
        free(shapes); free(header); free(scenery); free(terrain);
        return 0;
    }
    node_shapes(sections, count, model.left_verge, shapes);

    memcpy(header, template_tri, TRI_HEADER_SIZE);
    memset(header + NODES_OFFSET, 0, (size_t)NODE_SIZE * TRI_NODE_CAPACITY);
    memset(header + BRIGHTNESS_OFFSET, 0, (size_t)BRIGHTNESS_SIZE * TRI_SEGMENT_CAPACITY);
    put_u16(header + 4, 0);                               /* a point-to-point stage */
    const uint8_t *model_node = template_tri + NODES_OFFSET + NODE_SIZE;
    int32_t pos[3] = { 0, 0, 0 };
    for (int i = 0; i <= nodes; ++i) {
        write_node(header + NODES_OFFSET + NODE_SIZE * i, model_node, &shapes[i], pos, &model);
        const double heading = shapes[i].heading / ANGLE_TURN * 2 * M_PI;
        pos[0] += (int32_t)lround(TRACK_GEN_NODE_LENGTH * sin(heading));
        pos[1] += (int32_t)lround(TRACK_GEN_NODE_LENGTH * shapes[i].grade / 100.0);
        pos[2] += (int32_t)lround(TRACK_GEN_NODE_LENGTH * cos(heading));
    }
    put_u16(header + 0x18, (unsigned)(header[NODES_OFFSET + 0x14] | header[NODES_OFFSET + 0x15] << 8));
    put_u16(header + 0x20, (unsigned)(header[NODES_OFFSET + 0x18] | header[NODES_OFFSET + 0x19] << 8));

    uint8_t brightness[BRIGHTNESS_SIZE];
    if (tri_brightness(&tri, tri.segment_count > TEMPLATE_BRIGHTNESS ? TEMPLATE_BRIGHTNESS : tri.segment_count - 1, brightness) != TRI_OK) {
        memset(brightness, 0, sizeof brightness);
    }
    const uint32_t model_segment = tri.segment_count > TEMPLATE_SEGMENT ? TEMPLATE_SEGMENT : tri.segment_count - 1;
    for (int s = 0; s < segments; ++s) {
        memcpy(header + BRIGHTNESS_OFFSET + BRIGHTNESS_SIZE * s, brightness, sizeof brightness);
        write_strip(terrain + (size_t)s * TRI_SEGMENT_SIZE, parts.terrain + (size_t)model_segment * TRI_SEGMENT_SIZE,
                    shapes + s * TRI_NODES_PER_SEGMENT, &model);
    }
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
    free(shapes); free(header); free(scenery); free(terrain);
    return ok;
}
