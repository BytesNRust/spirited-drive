/* TRI writer tests: parts built by hand come back through tri_open with the
 * counts the writer derived; with CD files every TRI taken apart by tri_parts
 * and written back is identical. */
#include "formats/tri.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

enum { SEGMENTS_MAX = 4, SCENERY_BYTES = TRI_SCENERY_MAX * TRI_SCENERY_SIZE };

typedef struct {
    uint8_t header[TRI_HEADER_SIZE];
    uint8_t descriptors[3 * TRI_DESCRIPTOR_SIZE];
    uint8_t scenery[SCENERY_BYTES];
    uint8_t terrain[SEGMENTS_MAX * TRI_SEGMENT_SIZE];
    uint8_t trailer[12];
} Synthetic;

static void fill(Synthetic *s)
{
    memset(s, 0, sizeof *s);
    s->header[0] = TRI_VERSION;
    put_u32(s->header + 0x16218, 1000);  /* scenery_slots */
    memset(s->descriptors, 0x31, sizeof s->descriptors);
    put_u32(s->scenery, 0xffffffffu);    /* no placements: the terminator first */
    for (int i = 0; i < SEGMENTS_MAX; ++i) {
        uint8_t *seg = s->terrain + i * TRI_SEGMENT_SIZE;
        memcpy(seg, "TRKD", 4);
        put_u32(seg + 4, TRI_SEGMENT_SIZE - 12);
        seg[0x0c] = (uint8_t)(i + 1);
    }
    memcpy(s->trailer, "CRCF", 4);
}

static TriParts parts_of(const Synthetic *s, uint32_t segments, uint32_t descriptors, size_t trailer)
{
    TriParts p = { s->header, s->descriptors, descriptors, s->scenery, s->terrain, segments, s->trailer, trailer };
    return p;
}

static void test_derived_fields(void)
{
    static Synthetic s;
    static uint8_t out[sizeof(Synthetic) + 64];
    size_t len = 0;
    fill(&s);
    for (uint32_t segments = 1; segments <= SEGMENTS_MAX; ++segments) {
        for (uint32_t descriptors = 0; descriptors <= 3; ++descriptors) {
            const TriParts p = parts_of(&s, segments, descriptors, descriptors == 2 ? 12 : 0);
            CHECK_EQ_INT(tri_write(&p, out, sizeof out, &len), TRI_OK);
            CHECK_EQ_INT(len, tri_write_size(&p));
            Tri tri;
            CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
            CHECK_EQ_INT(tri.segment_count, segments);
            CHECK_EQ_INT(tri.descriptor_count, descriptors);
            CHECK_EQ_INT(tri.terrain_size, segments * TRI_SEGMENT_SIZE);
            CHECK_EQ_INT(tri.trailer_size, descriptors == 2 ? 12 : 0);
            TriSegment seg;
            CHECK_EQ_INT(tri_segment(&tri, segments - 1, &seg), TRI_OK);
            CHECK_EQ_INT(seg.unknown_0c, segments);            /* the last segment's marker byte survived */
            TriParts back;
            CHECK_EQ_INT(tri_parts(&tri, &back), TRI_OK);
            CHECK_EQ_INT(back.segment_count, segments);
            CHECK_EQ_INT(back.descriptor_count, descriptors);
        }
    }
}

static void test_errors(void)
{
    static Synthetic s;
    static uint8_t out[sizeof(Synthetic) + 64];
    size_t len = 0;
    fill(&s);
    TriParts p = parts_of(&s, 2, 3, 0);
    CHECK_EQ_INT(tri_write(&p, out, tri_write_size(&p) - 1, &len), TRI_ERR_BOUNDS);
    p.segment_count = TRI_SEGMENT_CAPACITY + 1;
    CHECK_EQ_INT(tri_write(&p, out, sizeof out, &len), TRI_ERR_BOUNDS);
}

static void check_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { CHECK(0); return; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)size), *out = malloc((size_t)size + 16);
    const int ok = buf != NULL && out != NULL && fread(buf, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    Tri tri;
    TriParts p;
    size_t len = 0;
    CHECK(ok && tri_open(&tri, buf, (size_t)size) == TRI_OK);
    CHECK(ok && tri_parts(&tri, &p) == TRI_OK);
    CHECK(ok && tri_write(&p, out, (size_t)size + 16, &len) == TRI_OK);
    if (!(ok && len == (size_t)size && memcmp(out, buf, (size_t)size) == 0)) {
        fprintf(stderr, "%s: does not write back identically\n", path);
        CHECK(0);
    }
    free(buf);
    free(out);
}

int main(int argc, char **argv)
{
    test_derived_fields();
    test_errors();
    for (int a = 1; a < argc; ++a) check_file(argv[a]);
    return TEST_REPORT();
}
