/* Chunk container tests on synthetic buffers: tree walking, absent children,
 * truncation at every length, bad offsets and nesting depth. The layout is the
 * one FUN_004511a4 (004511a4) relocates; see formats/chunk.h. */
#include "formats/chunk.h"
#include "test.h"

#include <stdint.h>

enum { SAMPLE_SIZE = 64 };

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* root wwww [20, 0 (absent), 48]
 *   @20 wwww [12] holding an empty SHPI at 20+12
 *   @48 ORIP of 16 bytes */
static void build_sample(uint8_t buf[SAMPLE_SIZE])
{
    memset(buf, 0, SAMPLE_SIZE);
    memcpy(buf, "wwww", 4);
    put_u32(buf + 4, 3);
    put_u32(buf + 8, 20);
    put_u32(buf + 12, 0);
    put_u32(buf + 16, 48);

    memcpy(buf + 20, "wwww", 4);
    put_u32(buf + 24, 1);
    put_u32(buf + 28, 12);
    memcpy(buf + 32, "SHPI", 4);
    put_u32(buf + 36, 16);   /* length; count at +8 is 0 */
    memcpy(buf + 44, "LN32", 4);

    memcpy(buf + 48, "ORIP", 4);
    put_u32(buf + 52, 16);
}

static void test_walk(void)
{
    uint8_t buf[SAMPLE_SIZE];
    build_sample(buf);
    Chunk root, child, grand;
    ChunkTotals totals;

    CHECK_EQ_INT(chunk_open(&root, buf, sizeof buf), CHUNK_OK);
    CHECK_EQ_INT(root.kind, CHUNK_KIND_HEADER);
    CHECK_EQ_INT(root.count, 3);
    CHECK_EQ_INT(chunk_survey(&root, &totals), CHUNK_OK);
    CHECK_EQ_INT(totals.headers, 2);
    CHECK_EQ_INT(totals.bitmaps, 1);
    CHECK_EQ_INT(totals.meshes, 1);
    CHECK_EQ_INT(totals.absent, 1);
    CHECK_EQ_INT(totals.max_depth, 2);

    CHECK_EQ_INT(chunk_child(&root, 0, &child), CHUNK_OK);
    CHECK_EQ_INT(child.kind, CHUNK_KIND_HEADER);
    CHECK_EQ_INT(child.size, 28);   /* runs to the next larger sibling offset */
    CHECK_EQ_INT(chunk_child(&child, 0, &grand), CHUNK_OK);
    CHECK_EQ_INT(grand.kind, CHUNK_KIND_BITMAP);
    CHECK_EQ_INT(grand.size, 16);
    CHECK(grand.data == buf + 32);

    CHECK_EQ_INT(chunk_child(&root, 1, &child), CHUNK_OK);
    CHECK_EQ_INT(child.kind, CHUNK_KIND_ABSENT);
    CHECK(child.data == NULL);

    CHECK_EQ_INT(chunk_child(&root, 2, &child), CHUNK_OK);
    CHECK_EQ_INT(child.kind, CHUNK_KIND_MESH);
    CHECK_EQ_INT(child.size, 16);

    CHECK_EQ_INT(chunk_child(&root, 3, &child), CHUNK_ERR_INDEX);
    CHECK_EQ_INT(chunk_child(&grand, 0, &child), CHUNK_ERR_KIND);

    Fsh fsh;
    CHECK_EQ_INT(chunk_bitmaps(&grand, &fsh), FSH_OK);
    CHECK_EQ_INT(fsh.count, 0);
    CHECK_EQ_INT(chunk_bitmaps(&root, &fsh), FSH_ERR_HEADER);
}

/* Cutting the buffer anywhere must be reported, never read past the end. */
static void test_truncation(void)
{
    uint8_t buf[SAMPLE_SIZE];
    build_sample(buf);
    for (size_t len = 0; len < SAMPLE_SIZE; len++) {
        Chunk root;
        ChunkTotals totals;
        ChunkResult r = chunk_open(&root, buf, len);
        if (r == CHUNK_OK) r = chunk_survey(&root, &totals);
        CHECK(r != CHUNK_OK);
    }
}

static void test_bad_tables(void)
{
    uint8_t buf[SAMPLE_SIZE];
    Chunk root, child;
    ChunkTotals totals;

    build_sample(buf);
    memcpy(buf, "wwwx", 4);
    CHECK_EQ_INT(chunk_open(&root, buf, sizeof buf), CHUNK_ERR_TAG);

    build_sample(buf);
    put_u32(buf + 4, 0x3FFFFFFF);   /* table larger than the data */
    CHECK_EQ_INT(chunk_open(&root, buf, sizeof buf), CHUNK_ERR_BOUNDS);

    build_sample(buf);
    chunk_open(&root, buf, sizeof buf);
    put_u32(buf + 8, 16);           /* child starts inside the offset table */
    CHECK_EQ_INT(chunk_child(&root, 0, &child), CHUNK_ERR_BOUNDS);
    put_u32(buf + 8, 64);           /* child starts at the end of the data */
    CHECK_EQ_INT(chunk_child(&root, 0, &child), CHUNK_ERR_BOUNDS);
    put_u32(buf + 8, 0xFFFFFFF0u);
    CHECK_EQ_INT(chunk_child(&root, 0, &child), CHUNK_ERR_BOUNDS);

    build_sample(buf);
    put_u32(buf + 52, 17);          /* mesh longer than the room left */
    chunk_open(&root, buf, sizeof buf);
    CHECK_EQ_INT(chunk_child(&root, 2, &child), CHUNK_ERR_BOUNDS);
    put_u32(buf + 52, 4);           /* shorter than its own length field */
    CHECK_EQ_INT(chunk_child(&root, 2, &child), CHUNK_ERR_BOUNDS);

    build_sample(buf);
    put_u32(buf + 8, 48);           /* two children at one offset: both fit */
    chunk_open(&root, buf, sizeof buf);
    CHECK_EQ_INT(chunk_child(&root, 0, &child), CHUNK_OK);

    build_sample(buf);
    put_u32(buf + 36, 40);          /* SHPI longer than its parent's extent */
    chunk_open(&root, buf, sizeof buf);
    CHECK_EQ_INT(chunk_survey(&root, &totals), CHUNK_ERR_BOUNDS);

    build_sample(buf);
    memcpy(buf + 32, "????", 4);
    chunk_open(&root, buf, sizeof buf);
    CHECK_EQ_INT(chunk_survey(&root, &totals), CHUNK_ERR_TAG);
}

/* A chain of single-child headers, each 12 bytes, ending in a tiny SHPI. */
static void test_depth(void)
{
    enum { LEVELS = CHUNK_MAX_DEPTH + 2, LEAF = 16 };
    uint8_t buf[LEVELS * 12 + LEAF];
    memset(buf, 0, sizeof buf);
    for (int i = 0; i < LEVELS; i++) {
        memcpy(buf + i * 12, "wwww", 4);
        put_u32(buf + i * 12 + 4, 1);
        put_u32(buf + i * 12 + 8, 12);
    }
    memcpy(buf + LEVELS * 12, "SHPI", 4);
    put_u32(buf + LEVELS * 12 + 4, LEAF);

    Chunk root;
    ChunkTotals totals;
    CHECK_EQ_INT(chunk_open(&root, buf, sizeof buf), CHUNK_OK);
    CHECK_EQ_INT(chunk_survey(&root, &totals), CHUNK_ERR_DEPTH);

    /* Same chain, one level short of the limit, is accepted. */
    uint8_t *shallow = buf + (LEVELS - CHUNK_MAX_DEPTH) * 12;
    CHECK_EQ_INT(chunk_open(&root, shallow, sizeof buf - (size_t)(shallow - buf)), CHUNK_OK);
    CHECK_EQ_INT(chunk_survey(&root, &totals), CHUNK_OK);
    CHECK_EQ_INT(totals.max_depth, CHUNK_MAX_DEPTH);
}

int main(void)
{
    test_walk();
    test_truncation();
    test_bad_tables();
    test_depth();
    return TEST_REPORT();
}
