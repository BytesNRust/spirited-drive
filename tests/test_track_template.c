/* The track template of our own: it opens as a TRI, is a straight stage with the cross-section and texture slots the header names, is the
 * same bytes every time, and track_gen makes a track from it. Its INFO (the sky's text) is read by the game's own reader (track_info_load),
 * which takes every value line, never runs out of text and sets what the horizon of a flat bitmap needs. */
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "game/race/track_info.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv64(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

static void check_structure(const uint8_t *file, size_t size)
{
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, file, size), TRI_OK);
    CHECK(!tri_is_circuit(&tri));
    CHECK_EQ_INT(tri.segment_count, TRACK_TEMPLATE_SEGMENTS);
    CHECK_EQ_INT(tri.descriptor_count, 64);
    CHECK_EQ_INT(tri.scenery_count, 0);   /* the list ends at once: no scenery */
    CHECK_EQ_INT(tri.node_count, TRACK_TEMPLATE_SEGMENTS * 4);

    for (uint32_t i = 0; i <= TRACK_TEMPLATE_SEGMENTS * 4; ++i) {
        TriNode n;
        CHECK_EQ_INT(tri_node(&tri, i, &n), TRI_OK);
        CHECK_EQ_INT(n.left_verge, TRACK_TEMPLATE_HALF_WIDTH);
        CHECK_EQ_INT(n.right_verge, TRACK_TEMPLATE_HALF_WIDTH);
        CHECK_EQ_INT(n.left_barrier, TRACK_TEMPLATE_BARRIER);
        CHECK_EQ_INT(n.right_barrier, TRACK_TEMPLATE_BARRIER);
        CHECK(n.flags[0] == 0x11 && n.flags[1] == 0x11 && n.flags[2] == 0x22);   /* lanes, walls: what the AI and the crash code read */
        CHECK_EQ_INT(n.property, 3);                                                /* the plain road */
        CHECK_EQ_INT(n.position.x, 0);
        CHECK_EQ_INT(n.position.y, 0);
        CHECK_EQ_INT(n.position.z, (long long)i * TRACK_GEN_NODE_LENGTH);
        CHECK_EQ_INT(n.orientation, 0);
        CHECK_EQ_INT(n.slope, 0);
        CHECK_EQ_INT(n.slant_a, 0);
        CHECK_EQ_INT(n.z_orientation, 32767);
        CHECK_EQ_INT(n.x_orientation, 0);
    }
    TriNode past;
    CHECK(tri_node(&tri, TRACK_TEMPLATE_SEGMENTS * 4 + 1, &past) != TRI_OK);   /* nothing after the closing node */

    for (uint32_t i = 0; i < TRACK_TEMPLATE_SEGMENTS; ++i) {
        TriSegment seg;
        CHECK_EQ_INT(tri_segment(&tri, i, &seg), TRI_OK);
        CHECK_EQ_INT(seg.fence, TRACK_TEMPLATE_FENCE);
        CHECK_EQ_INT(seg.fence & (TRI_FENCE_LEFT | TRI_FENCE_RIGHT), TRI_FENCE_LEFT | TRI_FENCE_RIGHT);   /* a fence each side */
        CHECK_EQ_INT(TRACK_GENERIC_FENCE_SLOT + (seg.fence & 0x1f), 32);   /* the texture slot the FAM's bank 10 letter C fills */
        const uint8_t want[10] = { TRACK_GENERIC_SLOT_ROAD, TRACK_GENERIC_SLOT_SHOULDER, TRACK_GENERIC_SLOT_VERGE, TRACK_GENERIC_SLOT_GRASS,
                                   TRACK_GENERIC_SLOT_GRASS, TRACK_GENERIC_SLOT_ROAD, TRACK_GENERIC_SLOT_SHOULDER, TRACK_GENERIC_SLOT_VERGE,
                                   TRACK_GENERIC_SLOT_GRASS, TRACK_GENERIC_SLOT_GRASS };
        CHECK_MEM(seg.texture, want, 10);
        for (int r = 0; r < 4; ++r) {
            const TriRow *row = &seg.rows[r];
            CHECK(row->middle.x == 0 && row->middle.y == 0 && row->middle.z == 0);
            for (int k = 0; k < 5; ++k) {   /* symmetric: the left chain is the right one mirrored across the road */
                CHECK_EQ_INT(row->left[k].x, -row->right[k].x);
                CHECK_EQ_INT(row->left[k].y, row->right[k].y);
                CHECK_EQ_INT(row->left[k].z, row->right[k].z);
            }
            /* the road's edge is where the verge byte says: a byte is 1/8 of a unit, a delta 1/128 of one, so 16 deltas to the byte */
            CHECK_EQ_INT(row->right[0].x, TRACK_TEMPLATE_HALF_WIDTH * 16);
            CHECK_EQ_INT(row->right[0].y, 0);
            long long reach = 0;
            for (int k = 0; k < 5; ++k) { CHECK(row->right[k].x > 0); reach += row->right[k].x; }
            CHECK(reach * 512 / 65536 >= 40);   /* at least 40 units of ground on each side */
        }
    }
    for (uint32_t i = 0; i < TRACK_TEMPLATE_SEGMENTS; ++i) {
        uint8_t brightness[3];
        CHECK_EQ_INT(tri_brightness(&tri, i, brightness), TRI_OK);
        CHECK(brightness[0] == 32 && brightness[1] == 27 && brightness[2] == 34);   /* the entry track_gen copies to every strip */
    }
}

static void check_same_bytes_each_time(const uint8_t *a, size_t asize)
{
    uint8_t *b = NULL;
    size_t bsize = 0;
    ExchangeError err = {0};
    CHECK(track_template_generic(&b, &bsize, &err));
    CHECK_EQ_INT(bsize, asize);
    if (b != NULL && bsize == asize) CHECK_MEM(a, b, asize);
    free(b);
}

static void check_generate_from_it(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    const char *layout = "section 10\nsection 20 turn=90 grade=4 bank=5 width=50\nsection 10 grade=0 bank=0\n";
    CHECK(track_generate(layout, strlen(layout), tmpl, tsize, &out, &len, &err));
    if (out == NULL) return;
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
    CHECK_EQ_INT(tri.segment_count, 40);
    TriSegment seg;
    CHECK_EQ_INT(tri_segment(&tri, 20, &seg), TRI_OK);   /* the generated strips keep the template's textures and are widened by width */
    CHECK_EQ_INT(seg.texture[0], TRACK_GENERIC_SLOT_ROAD);
    CHECK_EQ_INT(seg.texture[9], TRACK_GENERIC_SLOT_GRASS);
    TriNode node;
    CHECK_EQ_INT(tri_node(&tri, 80, &node), TRI_OK);   /* the node bytes the generator copies from the template's keep the lanes, walls and property */
    CHECK(node.flags[0] == 0x11 && node.flags[1] == 0x11 && node.flags[2] == 0x22);
    CHECK_EQ_INT(node.property, 3);
    CHECK(node.left_verge > TRACK_TEMPLATE_HALF_WIDTH);   /* widened by the layout */
    CHECK_EQ_INT(seg.rows[0].right[0].x, 16 * node.left_verge);   /* the road's edge is at the node's verge byte (rows 0 of strip 20 is node 80) */
    free(out);
}

/* the INFO as the game's reader takes it: it counts the lines the reader asks for and the value lines it is handed, and notes a read past the end */
typedef struct {
    const char *p, *end;
    int value_lines, past_end, longest, tint_calls;
    int32_t tint[3][5];   /* the tint of time of day 0 and 2 (one call each) */
} Reading;

static int is_value_line(const char *line) { return line[0] != '#' && line[0] != '\n' && line[0] != '\r'; }

static int32_t reading_open(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static void reading_close(void *ctx, int32_t file) { (void)ctx, (void)file; }

static int reading_gets(void *ctx, char *line, int32_t size, int32_t file)   /* fgets: up to a newline, at most size - 1 characters */
{
    (void)file;
    Reading *r = ctx;
    if (r->p >= r->end) {
        r->past_end++;
        return 0;
    }
    int32_t n = 0;
    while (r->p < r->end && n + 1 < size) {
        const char c = *r->p++;
        line[n++] = c;
        if (c == '\n') break;
    }
    line[n] = '\0';
    if (n > r->longest) r->longest = n;
    if (is_value_line(line)) r->value_lines++;
    return 1;
}

static void reading_tint(void *ctx, int32_t red, int32_t green, int32_t blue, int32_t saturation, int32_t brightness)
{
    Reading *r = ctx;
    if (r->tint_calls < 3) {
        const int32_t t[5] = { red, green, blue, saturation, brightness };
        memcpy(r->tint[r->tint_calls], t, sizeof t);
    }
    r->tint_calls++;
}

static void read_info(const char *text, size_t len, int time_of_day, Reading *r, TrackInfo *info)
{
    memset(r, 0, sizeof *r);
    memset(info, 0, sizeof *info);
    r->p = text, r->end = text + len;
    const TrackInfoOps ops = { r, reading_open, reading_gets, reading_close, reading_tint };
    const TrackInfoInputs in = { "ZZ1", "", 1, time_of_day, 1 };   /* the file's values, the full-size screen: nothing scaled */
    track_info_load(&ops, &in, info);
}

static void check_info(void)
{
    enum { VALUES = 8 + 2 + 3 + 15 + 1 };   /* what the reader takes from the file: 8 decimals, 2 colours, 3 ring words, 3 x 5 tints, the final flag */
    size_t len = 0;
    const char *text = track_template_info(&len);
    CHECK(text != NULL);
    if (text == NULL) return;
    CHECK(len > 0 && len == strlen(text));   /* text with no NUL inside, its length the one given */
    int bare = 0, longest = 0, line = 0;
    for (size_t i = 0; i < len; ++i) {   /* the game's files end their lines CR LF; a line the reader's buffer would cut in two reads as two */
        line++;
        if (text[i] == '\n') {
            if (i == 0 || text[i - 1] != '\r') bare++;
            if (line > longest) longest = line;
            line = 0;
        } else if (text[i] == '\r' && (i + 1 >= len || text[i + 1] != '\n')) {
            bare++;
        }
    }
    CHECK_EQ_INT(bare, 0);
    CHECK(longest < TRACK_INFO_LINE);   /* the reader asks fgets for TRACK_INFO_LINE: at most that less one a line */
    CHECK(line == 0);                   /* it ends with a line end */

    Reading r;
    TrackInfo info;
    read_info(text, len, 0, &r, &info);
    CHECK_EQ_INT(r.value_lines, VALUES);   /* every value line is handed over and no other: the file holds as many as the reader takes */
    CHECK_EQ_INT(r.past_end, 0);           /* never a read past the end: a short file would have its last line stand for the rest */
    CHECK(r.longest < TRACK_INFO_LINE);
    CHECK_EQ_INT(info.vert_offset_near, 0);   /* the sky: a level horizon (vert and z offsets 0), a ring height and radius the CD's A-to-B tracks have */
    CHECK_EQ_INT(info.vert_offset_far, 0);
    CHECK_EQ_INT(info.z_offset_near, 0);
    CHECK_EQ_INT(info.z_offset_far, 0);
    CHECK_EQ_INT(info.height_near, 0);
    CHECK_EQ_INT(info.height_far, 48 << 16);   /* decimals read as 16.16 */
    CHECK_EQ_INT(info.radius_near, 0);
    CHECK_EQ_INT(info.radius_far, 96 << 16);
    CHECK_EQ_INT(info.colour_top, 0 << 10 | 1 << 5 | 2);      /* "0 1 2" packed r << 10 | g << 5 | b */
    CHECK_EQ_INT(info.colour_bottom, 3 << 10 | 4 << 5 | 5);
    CHECK_EQ_INT(info.ring, 0);   /* a flat horizon bitmap (the template's FAM holds one), not a 3D ring: the ring type and the final flag are both 0 */
    CHECK_EQ_INT(info.final_flag, 0);
    CHECK_EQ_INT(info.ring_y_offset, -60);
    CHECK_EQ_INT(info.ring_height, 112);
    const int32_t groups[3][5] = { { 292, 216, 0, 0, 255 << 8 }, { 255, 255, 255, 0, 255 << 8 }, { 295, 184, 14, -1286, 289 << 8 } };
    for (int g = 0; g < 3; ++g)   /* three groups of five, the fifth of each << 8 */
        for (int k = 0; k < 5; ++k) CHECK_EQ_INT(info.time_of_day[k][g], groups[g][k]);
    CHECK_EQ_INT(r.tint_calls, 1);   /* the tint of the time of day asked: the group's five */
    CHECK_MEM(r.tint[0], groups[0], sizeof groups[0]);
    read_info(text, len, 2, &r, &info);
    CHECK_EQ_INT(r.value_lines, VALUES);   /* dusk reads the file too (time of day 1, the midday art's, is the one that does not) */
    CHECK_MEM(r.tint[0], groups[2], sizeof groups[2]);

    TrackInfo parsed;
    CHECK(track_info_parse(&parsed, text, len));   /* the viewer's parse of the same text: the same values */
    CHECK(parsed.ring_height == 112 && parsed.height_far == 48 << 16 && parsed.colour_bottom == (3 << 10 | 4 << 5 | 5));

    /* the check can fail: a text a value line short is read with one value line less, and then past its end */
    const char *last = text + len;
    int lines_seen = 0;
    while (last > text && lines_seen < 2) {   /* back over the closing blank line and the last value line */
        --last;
        if (last > text && last[-1] == '\n') lines_seen++;
    }
    read_info(text, (size_t)(last - text), 0, &r, &info);
    CHECK_EQ_INT(r.value_lines, VALUES - 1);
    CHECK(r.past_end > 0);
}

int main(void)
{
    uint8_t *file = NULL;
    size_t size = 0;
    ExchangeError err = {0};
    CHECK(track_template_generic(&file, &size, &err));
    if (file == NULL) return TEST_REPORT();
    check_structure(file, size);
    check_same_bytes_each_time(file, size);
    check_generate_from_it(file, size);
    check_info();
    CHECK(fnv64(file, size) == 0x626720e610226d90ull);   /* the golden: whole numbers all the way, so one hash for every machine */
    free(file);
    return TEST_REPORT();
}
