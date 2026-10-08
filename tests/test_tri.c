/* TRI (track geometry) tests. Layout and counting follow the loader 0043e4e0
 * and the chunk lookup 0043e244. With file arguments (registered by CMake when
 * the CD data is present) every file is also parsed and its counts are
 * recomputed independently. */
#include "formats/tri.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

enum {
    HEADER_BLOCK = 0x1621C,
    NODES_AT = 0x98C,
    NODE_BYTES = 36,
    DESC_AT = HEADER_BLOCK + 12,
    SEG_BYTES = 288,
    SYN_DESCS = 2,
    SYN_RECORD_SLOTS = 4,
    SYN_SEGMENTS = 3
};

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    put_u16(p, (uint16_t)v);
    put_u16(p + 2, (uint16_t)(v >> 16));
}

static uint32_t u32_at(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Synthetic file: 3 segments, 2 descriptors, room for 4 scenery records of
 * which 2 are real and the third is the -1 terminator. */
static size_t syn_chunk_size(void)
{
    return 12 + SYN_DESCS * 16 + SYN_RECORD_SLOTS * 16;
}

static size_t syn_terrain_at(void)
{
    return HEADER_BLOCK + syn_chunk_size();
}

static uint8_t *build_sample(size_t *size)
{
    *size = syn_terrain_at() + SYN_SEGMENTS * SEG_BYTES;
    uint8_t *f = calloc(1, *size);
    if (f == NULL) return NULL;
    f[0] = TRI_VERSION;
    put_u16(f + 4, 0);                 /* point-to-point */
    put_u16(f + 6, SYN_SEGMENTS);
    put_u16(f + 0x18, 7);
    put_u16(f + 0x20, 9);
    put_u32(f + 0x24, SYN_SEGMENTS * SEG_BYTES);
    for (int i = 0; i < SYN_SEGMENTS; i++) put_u32(f + 0x2C + 4 * i, (uint32_t)(i * SEG_BYTES));

    /* node 1 */
    uint8_t *n = f + NODES_AT + NODE_BYTES;
    n[0] = 40; n[1] = 41; n[2] = 64; n[3] = 65;
    n[4] = 0x11; n[5] = 0x12; n[6] = 0x22; n[7] = 3;
    put_u32(n + 8, 65);
    put_u32(n + 12, (uint32_t)-524);
    put_u32(n + 16, 0x5FEF9);
    put_u16(n + 0x14, 3);
    put_u16(n + 0x16, (uint16_t)-4);
    put_u16(n + 0x18, 0x1000);
    put_u16(n + 0x1C, 0x7FFE);
    put_u16(n + 0x1E, 5);
    put_u16(n + 0x20, (uint16_t)-5);

    f[0x15B0C + 3 * 2] = 11; f[0x15B0C + 3 * 2 + 1] = 22; f[0x15B0C + 3 * 2 + 2] = 33;

    put_u32(f + 0x16214, SYN_DESCS);
    put_u32(f + 0x16218, SYN_RECORD_SLOTS);
    memcpy(f + HEADER_BLOCK, "SJBO", 4);
    put_u32(f + HEADER_BLOCK + 4, (uint32_t)syn_chunk_size());

    uint8_t *d = f + DESC_AT + 16;     /* descriptor 1 */
    d[0] = 4; d[1] = 6; d[2] = 5; d[3] = 6;
    put_u16(d + 6, 1);
    put_u16(d + 14, (uint16_t)-3);

    uint8_t *s = f + DESC_AT + SYN_DESCS * 16;
    put_u32(s, 12);                    /* record 0 */
    s[4] = 1; s[5] = 200; s[6] = 1; s[7] = 2; s[8] = 3; s[9] = 4;
    put_u16(s + 10, (uint16_t)-1); put_u16(s + 12, 2); put_u16(s + 14, 3);
    put_u32(s + 16, 5000);             /* record 1: node above the count */
    put_u32(s + 32, 0xFFFFFFFFu);      /* terminator */
    put_u32(s + 48, 77);               /* ignored: after the terminator */

    for (int i = 0; i < SYN_SEGMENTS; i++) {
        uint8_t *c = f + syn_terrain_at() + (size_t)i * SEG_BYTES;
        memcpy(c, "TRKD", 4);
        put_u32(c + 4, 276);
        c[0xC] = 9;
        c[0xD] = (uint8_t)(TRI_FENCE_LEFT | 5);
        for (int t = 0; t < 10; t++) c[0xE + t] = (uint8_t)(i * 10 + t);
        for (int r = 0; r < 4; r++) {
            for (int k = 0; k < 33; k++) {
                put_u16(c + 0x18 + r * 66 + 2 * k, (uint16_t)(int16_t)(i * 1000 + r * 100 - k));
            }
        }
    }
    return f;
}

static void test_fields(void)
{
    size_t size;
    uint8_t *f = build_sample(&size);
    CHECK(f != NULL);
    if (f == NULL) return;
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_OK);
    CHECK_EQ_INT(tri.segment_count, SYN_SEGMENTS);
    CHECK_EQ_INT(tri.node_count, SYN_SEGMENTS * 4);
    CHECK_EQ_INT(tri_is_circuit(&tri), 0);
    CHECK_EQ_INT(tri.ring_segments, 0x8000);
    CHECK_EQ_INT(tri.ring_nodes, 0x20000);
    CHECK_EQ_INT(tri.node0_slope, 7);
    CHECK_EQ_INT(tri.node0_orientation, 9);
    CHECK_EQ_INT(tri.descriptor_count, SYN_DESCS);
    CHECK_EQ_INT(tri.scenery_slots, SYN_RECORD_SLOTS);
    CHECK_EQ_INT(tri.scenery_count, 2);
    CHECK_EQ_INT(tri.trailer_size, 0);

    TriNode n;
    CHECK_EQ_INT(tri_node(&tri, 1, &n), TRI_OK);
    CHECK_EQ_INT(n.left_verge, 40);
    CHECK_EQ_INT(n.right_barrier, 65);
    CHECK_EQ_INT(n.flags[1], 0x12);
    CHECK_EQ_INT(n.property, 3);
    CHECK_EQ_INT(n.position.x, 65);
    CHECK_EQ_INT(n.position.y, -524);
    CHECK_EQ_INT(n.position.z, 0x5FEF9);
    CHECK_EQ_INT(n.slope, 3);
    CHECK_EQ_INT(n.slant_a, -4);
    CHECK_EQ_INT(n.orientation, 0x1000);
    CHECK_EQ_INT(n.z_orientation, 0x7FFE);
    CHECK_EQ_INT(n.slant_b, 5);
    CHECK_EQ_INT(n.x_orientation, -5);
    CHECK_EQ_INT(tri_node(&tri, tri.node_count, &n), TRI_OK);  /* closing node */
    CHECK_EQ_INT(tri_node(&tri, tri.node_count + 1, &n), TRI_ERR_BOUNDS);

    TriDescriptor d;
    CHECK_EQ_INT(tri_descriptor(&tri, 1, &d), TRI_OK);
    CHECK_EQ_INT(d.flags, 4);
    CHECK_EQ_INT(d.type, 6);
    CHECK_EQ_INT(d.resource_id, 5);
    CHECK_EQ_INT(d.resource2_id, 6);
    CHECK_EQ_INT(d.width, 1);
    CHECK_EQ_INT(d.height, -3);
    CHECK_EQ_INT(tri_descriptor(&tri, SYN_DESCS, &d), TRI_ERR_BOUNDS);

    TriScenery s;
    CHECK_EQ_INT(tri_scenery(&tri, 0, &s), TRI_OK);
    CHECK_EQ_INT(s.node, 12);
    CHECK_EQ_INT(s.descriptor, 1);
    CHECK_EQ_INT(s.orientation, 200);
    CHECK_EQ_INT(s.flags[3], 4);
    CHECK_EQ_INT(s.x, -1);
    CHECK_EQ_INT(s.y, 2);
    CHECK_EQ_INT(s.z, 3);
    CHECK_EQ_INT(tri_scenery(&tri, 1, &s), TRI_OK);
    CHECK_EQ_INT(s.node, 5000);
    CHECK_EQ_INT(tri_scenery(&tri, 2, &s), TRI_ERR_BOUNDS);

    TriSegment seg;
    CHECK_EQ_INT(tri_segment(&tri, 2, &seg), TRI_OK);
    CHECK_EQ_INT(seg.length, 276);
    CHECK_EQ_INT(seg.unknown_0c, 9);
    CHECK_EQ_INT(seg.fence & TRI_FENCE_LEFT, TRI_FENCE_LEFT);
    CHECK_EQ_INT(seg.fence & TRI_FENCE_TEXTURE_MASK, 5);
    CHECK_EQ_INT(seg.texture[9], 29);
    CHECK_EQ_INT(seg.rows[0].middle.x, 2000);
    CHECK_EQ_INT(seg.rows[0].middle.z, 1998);
    CHECK_EQ_INT(seg.rows[0].right[0].x, 1997);   /* k = 3 */
    CHECK_EQ_INT(seg.rows[0].right[4].z, 2000 - 17);
    CHECK_EQ_INT(seg.rows[0].left[0].x, 2000 - 18);
    CHECK_EQ_INT(seg.rows[3].left[4].z, 2300 - 32);
    CHECK_EQ_INT(tri_segment(&tri, 3, &seg), TRI_ERR_BOUNDS);

    uint8_t b[3];
    CHECK_EQ_INT(tri_brightness(&tri, 2, b), TRI_OK);
    CHECK_EQ_INT(b[0], 11);
    CHECK_EQ_INT(b[2], 33);
    CHECK_EQ_INT(tri_brightness(&tri, 600, b), TRI_ERR_BOUNDS);
    free(f);
}

static void test_circuit_and_trailer(void)
{
    size_t size;
    uint8_t *f = build_sample(&size);
    CHECK(f != NULL);
    if (f == NULL) return;
    uint8_t *g = realloc(f, size + 12);
    CHECK(g != NULL);
    if (g == NULL) { free(f); return; }
    memcpy(g + size, "CRCF\x0c\0\0\0\xe9\x0c\0\0", 12);
    put_u16(g + 4, 128);
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, g, size + 12), TRI_OK);
    CHECK_EQ_INT(tri_is_circuit(&tri), 1);
    CHECK_EQ_INT(tri.ring_segments, 128);
    CHECK_EQ_INT(tri.ring_nodes, 512);
    CHECK_EQ_INT(tri.trailer_size, 12);
    free(g);
}

static void test_scenery_limit(void)
{
    /* Without a terminator the count stops at the end of the block. */
    size_t size;
    uint8_t *f = build_sample(&size);
    CHECK(f != NULL);
    if (f == NULL) return;
    put_u32(f + DESC_AT + SYN_DESCS * 16 + 32, 1);
    put_u32(f + DESC_AT + SYN_DESCS * 16 + 48, 2);
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_OK);
    CHECK_EQ_INT(tri.scenery_count, SYN_RECORD_SLOTS);
    free(f);
}

static void test_errors(void)
{
    size_t size;
    uint8_t *f = build_sample(&size);
    CHECK(f != NULL);
    if (f == NULL) return;
    Tri tri;

    /* Every truncation below the full size fails, none crashes. */
    const size_t cuts[] = { 0, 1, 0x98C, HEADER_BLOCK - 1, HEADER_BLOCK + 11, HEADER_BLOCK + 12,
                            syn_terrain_at() - 1, syn_terrain_at(), syn_terrain_at() + SEG_BYTES,
                            size - 1 };
    for (size_t i = 0; i < sizeof cuts / sizeof cuts[0]; i++) {
        CHECK(tri_open(&tri, f, cuts[i]) != TRI_OK);
    }
    CHECK_EQ_INT(tri_open(&tri, f, 0), TRI_ERR_HEADER);
    CHECK_EQ_INT(tri_open(&tri, f, size - 1), TRI_ERR_BOUNDS);

    f[0] = 0x10;
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_HEADER);
    f[0] = TRI_VERSION;

    put_u16(f + 6, TRI_SEGMENT_CAPACITY + 1);
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_HEADER);
    put_u16(f + 6, SYN_SEGMENTS);

    f[HEADER_BLOCK] = 'X';
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_CHUNK);
    f[HEADER_BLOCK] = 'S';

    put_u32(f + HEADER_BLOCK + 4, 0xFFFFFFF0u);
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_BOUNDS);
    put_u32(f + HEADER_BLOCK + 4, (uint32_t)syn_chunk_size());

    put_u32(f + 0x16214, 1000);        /* more descriptors than the chunk holds */
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_BOUNDS);
    put_u32(f + 0x16214, SYN_DESCS);

    put_u32(f + 0x24, SYN_SEGMENTS * SEG_BYTES + 1);
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_BOUNDS);
    put_u32(f + 0x24, SYN_SEGMENTS * SEG_BYTES);

    put_u32(f + 0x2C + 4, SYN_SEGMENTS * SEG_BYTES);  /* offset past the block */
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_BOUNDS);
    put_u32(f + 0x2C + 4, SEG_BYTES);

    f[syn_terrain_at() + SEG_BYTES] = 'Q';
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_CHUNK);
    f[syn_terrain_at() + SEG_BYTES] = 'T';

    put_u32(f + syn_terrain_at() + 4, 100);            /* chunk too short for 4 rows */
    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_ERR_CHUNK);
    put_u32(f + syn_terrain_at() + 4, 276);

    CHECK_EQ_INT(tri_open(&tri, f, size), TRI_OK);
    free(f);
}

static void test_brightness_scale(void)
{
    /* 0043e4e0: (v * s) >> 16 plus the rounding bit 15. */
    CHECK_EQ_INT(tri_brightness_scale(100, 0x10000), 100);
    CHECK_EQ_INT(tri_brightness_scale(100, 0x8000), 50);
    CHECK_EQ_INT(tri_brightness_scale(3, 0x8000), 2);     /* 1.5 rounds up */
    CHECK_EQ_INT(tri_brightness_scale(3, 0x7FFF), 1);
    CHECK_EQ_INT(tri_brightness_scale(0, 0x20000), 0);
}

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *buf = n > 0 ? malloc((size_t)n) : NULL;
    if (buf != NULL && fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(fp);
    *size = buf != NULL ? (size_t)n : 0;
    return buf;
}

static int has_suffix(const char *s, const char *suffix)
{
    const size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

/* Counts recomputed straight from the bytes, the way 0043e4e0 does. */
static void check_real_file(const char *path)
{
    size_t size;
    uint8_t *f = read_file(path, &size);
    CHECK(f != NULL);
    if (f == NULL) return;
    Tri tri;
    const TriResult r = tri_open(&tri, f, size);
    if (r != TRI_OK) fprintf(stderr, "%s: %s\n", path, tri_result_str(r));
    CHECK_EQ_INT(r, TRI_OK);
    if (r != TRI_OK) { free(f); return; }

    const uint16_t s4 = (uint16_t)(f[4] | (f[5] << 8));
    const uint16_t s6 = (uint16_t)(f[6] | (f[7] << 8));
    CHECK_EQ_INT(tri.node_count, s6 * 4);
    CHECK_EQ_INT(tri_is_circuit(&tri), s4 != 0);
    CHECK_EQ_INT(tri.ring_nodes, (s4 != 0 ? s4 : 0x8000) * 4);
    if (s4 != 0) CHECK_EQ_INT(s4, s6);
    CHECK_EQ_INT(tri.descriptor_count, 64);
    CHECK_EQ_INT(tri.scenery_slots, 1000);
    CHECK_EQ_INT(tri.terrain_offset, 0x1A4A8);
    CHECK_EQ_INT(tri.terrain_size, (uint32_t)s6 * 288);
    CHECK_EQ_INT(tri.terrain_offset + tri.terrain_size + tri.trailer_size, size);

    uint32_t records = 0;
    while (records < 1000 && u32_at(f + 0x16228 + 64 * 16 + records * 16) != 0xFFFFFFFFu) records++;
    CHECK_EQ_INT(tri.scenery_count, records);
    for (uint32_t i = 0; i < tri.scenery_count; i++) {
        TriScenery s;
        CHECK_EQ_INT(tri_scenery(&tri, i, &s), TRI_OK);
        CHECK(s.node >= 0);
        CHECK(s.descriptor < tri.descriptor_count);
    }
    for (uint32_t i = 0; i < tri.segment_count; i++) {
        TriSegment seg;
        CHECK_EQ_INT(tri_segment(&tri, i, &seg), TRI_OK);
        CHECK_EQ_INT(seg.length, 276);
    }
    for (uint32_t i = 0; i < tri.node_count; i++) {
        TriNode n;
        CHECK_EQ_INT(tri_node(&tri, i, &n), TRI_OK);
        CHECK(n.left_barrier != 0 || n.right_barrier != 0);  /* every used node has a width */
    }
    if (has_suffix(path, "AL1.TRI")) {
        TriNode n;
        CHECK_EQ_INT(tri.node_count, 2080);
        CHECK_EQ_INT(tri_node(&tri, 0, &n), TRI_OK);
        CHECK_EQ_INT(n.left_verge, 40);
        CHECK_EQ_INT(n.right_barrier, 64);
        CHECK_EQ_INT(n.property, 3);
        CHECK_EQ_INT(tri_node(&tri, 1, &n), TRI_OK);
        CHECK_EQ_INT(n.position.x, 65);
        CHECK_EQ_INT(n.position.y, 524);
        CHECK_EQ_INT(n.position.z, 0x5FEF9);
    }
    free(f);
}

int main(int argc, char **argv)
{
    test_fields();
    test_circuit_and_trailer();
    test_scenery_limit();
    test_errors();
    test_brightness_scale();
    for (int i = 1; i < argc; i++) check_real_file(argv[i]);
    return TEST_REPORT();
}
