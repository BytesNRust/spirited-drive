#include "formats/track_template.h"

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/track_gen.h"
#include "formats/tri.h"

#include <stdlib.h>
#include <string.h>

enum {
    NODES_OFFSET = 0x98C,
    NODE_SIZE = 36,
    BRIGHTNESS_OFFSET = 0x15B0C,
    BRIGHTNESS_SIZE = 3,
    ROW_SIZE = 66,
    ROW_BASE = 0x18,
    VECTOR_ONE = 32767,
    DESCRIPTORS = 64,         /* what every file of the CD declares */
    NODE_PROPERTY = 3,        /* every stage of the CD: the plain road */
    NODE_FLAGS_LANES = 0x11,  /* one lane open each side for the cars of the race, as on AL1 */
    NODE_FLAGS_WALLS = 0x11,
    NODE_FLAGS_OTHER = 0x22,
    HEADER_SCALAR_0A = 6,     /* 6 in every file */
    HEADER_SCALAR_28 = 0x30,  /* 0x2d..0x3f on the CD; read only for a circuit's animated texture */
    BRIGHT_0 = 32, BRIGHT_1 = 27, BRIGHT_2 = 34   /* the table's entry the generator copies to every strip (the CD's AL1 gives these at 10) */
};

/* The right chain of a row from the road's centre outwards, each point a delta from the one before, in 1/128 of a unit: the road's edge at
 * the verge byte (5 units), a metre-ish of shoulder to the barrier, then the verge dropping away and two bands of grass. */
static const int16_t CHAIN[5][3] = {
    { TRACK_TEMPLATE_HALF_WIDTH * 16, 0, 0 },
    { (TRACK_TEMPLATE_BARRIER - TRACK_TEMPLATE_HALF_WIDTH) * 16, -32, 0 },
    { 1280, -128, 0 },
    { 2560, -64, 0 },
    { 3840, 0, 0 }
};

/* The slot of each of a strip's 10 bands: the right chain from the centre outwards, then the left one. */
static const uint8_t BANDS[5] = {
    TRACK_GENERIC_SLOT_ROAD, TRACK_GENERIC_SLOT_SHOULDER, TRACK_GENERIC_SLOT_VERGE, TRACK_GENERIC_SLOT_GRASS, TRACK_GENERIC_SLOT_GRASS
};

static void put_u16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    put_u16(p, v & 0xffff);
    put_u16(p + 2, v >> 16);
}

static void put_point(uint8_t *p, int x, int y, int z)
{
    put_u16(p, (unsigned)x & 0xffff);
    put_u16(p + 2, (unsigned)y & 0xffff);
    put_u16(p + 4, (unsigned)z & 0xffff);
}

/* a node of the straight road: heading, slope and bank 0, the orientation vector along +z, the position one node length further each time */
static void write_node(uint8_t *dst, int index)
{
    dst[0] = dst[1] = TRACK_TEMPLATE_HALF_WIDTH;
    dst[2] = dst[3] = TRACK_TEMPLATE_BARRIER;
    dst[4] = NODE_FLAGS_LANES;
    dst[5] = NODE_FLAGS_WALLS;
    dst[6] = NODE_FLAGS_OTHER;
    dst[7] = NODE_PROPERTY;
    put_u32(dst + 8, 0);
    put_u32(dst + 12, 0);
    put_u32(dst + 16, (uint32_t)((int64_t)index * TRACK_GEN_NODE_LENGTH));
    put_u16(dst + 0x1c, VECTOR_ONE);
}

static void write_strip(uint8_t *seg)
{
    memcpy(seg, "TRKD", 4);
    put_u32(seg + 4, TRI_SEGMENT_SIZE - 12);
    seg[0xd] = TRACK_TEMPLATE_FENCE;
    for (int s = 0; s < 5; ++s) {
        seg[0xe + s] = BANDS[s];
        seg[0xe + 5 + s] = BANDS[s];
    }
    for (int r = 0; r < TRI_NODES_PER_SEGMENT; ++r) {
        uint8_t *row = seg + ROW_BASE + ROW_SIZE * r;
        for (int k = 0; k < 5; ++k) {
            put_point(row + 6 * (1 + k), CHAIN[k][0], CHAIN[k][1], CHAIN[k][2]);
            put_point(row + 6 * (6 + k), -CHAIN[k][0], CHAIN[k][1], CHAIN[k][2]);
        }
    }
}

/* The INFO's keys are the game's (its lines, in its order, CR LF); the values are ours: a level horizon in a sky blue, the road's light by
 * the hours of the day */
static const char INFO[] =
    "#vert offset near\r\n0\r\n\r\n#vert offset far\r\n0\r\n\r\n"
    "#zoffset near\r\n0\r\n\r\n#zoffset far\r\n0\r\n\r\n"
    "#height near\r\n0\r\n\r\n#height far\r\n48\r\n\r\n"
    "#radius near\r\n0\r\n\r\n#radius far\r\n96\r\n\r\n"
    "#colour top\r\n0 1 2\r\n\r\n#colour bottom\r\n3 4 5\r\n\r\n"
    "#ring type? (1 is TRUE, 0 is FALSE)\r\n0\r\n\r\n"
    "#ring y offset (bigger places horizon further down)\r\n-60\r\n\r\n"
    "#ring height\r\n112\r\n\r\n"
    "#RGB TIME OF DAY VALUES\r\n\r\n"
    "292\r\n216\r\n0\r\n0\r\n255\r\n\r\n255\r\n255\r\n255\r\n0\r\n255\r\n\r\n"
    "295\r\n184\r\n14\r\n-1286\r\n289\r\n\r\n0\r\n\r\n";

const char *track_template_info(size_t *len)
{
    *len = sizeof INFO - 1;
    return INFO;
}

int track_template_generic(uint8_t **out, size_t *out_len, ExchangeError *err)
{
    *out = NULL;
    const int segments = TRACK_TEMPLATE_SEGMENTS;
    uint8_t *header = calloc(1, TRI_HEADER_SIZE);
    uint8_t *descriptors = calloc(DESCRIPTORS, TRI_DESCRIPTOR_SIZE);
    uint8_t *scenery = malloc((size_t)TRI_SCENERY_MAX * TRI_SCENERY_SIZE);
    uint8_t *terrain = calloc((size_t)segments, TRI_SEGMENT_SIZE);
    uint8_t *file = NULL;
    int ok = 0;
    if (header == NULL || descriptors == NULL || scenery == NULL || terrain == NULL) {
        exchange_fail(err, "out of memory");
        goto done;
    }
    header[0] = TRI_VERSION;
    put_u16(header + 0x0a, HEADER_SCALAR_0A);
    put_u32(header + 0x28, HEADER_SCALAR_28);
    for (int i = 0; i <= segments * TRI_NODES_PER_SEGMENT; ++i) write_node(header + NODES_OFFSET + (size_t)NODE_SIZE * i, i);
    for (int s = 0; s < TRI_SEGMENT_CAPACITY; ++s) {
        header[BRIGHTNESS_OFFSET + BRIGHTNESS_SIZE * s] = BRIGHT_0;
        header[BRIGHTNESS_OFFSET + BRIGHTNESS_SIZE * s + 1] = BRIGHT_1;
        header[BRIGHTNESS_OFFSET + BRIGHTNESS_SIZE * s + 2] = BRIGHT_2;
    }
    memset(scenery, 0xff, (size_t)TRI_SCENERY_MAX * TRI_SCENERY_SIZE);   /* the first record's -1 ends the list: no scenery */
    for (int s = 0; s < segments; ++s) write_strip(terrain + (size_t)s * TRI_SEGMENT_SIZE);

    const TriParts parts = { header, descriptors, DESCRIPTORS, scenery, terrain, (uint32_t)segments, NULL, 0 };
    const size_t size = tri_write_size(&parts);
    file = malloc(size);
    if (file == NULL || tri_write(&parts, file, size, out_len) != TRI_OK) {
        exchange_fail(err, "cannot assemble the template TRI");
        free(file);
        goto done;
    }
    *out = file;
    ok = 1;
done:
    free(header); free(descriptors); free(scenery); free(terrain);
    return ok;
}

int track_template_check_fam(const uint8_t *fam, size_t size, ExchangeError *err)
{
    Chunk root, terrain, bank;
    Fsh fsh;
    uint32_t at;
    if (chunk_open(&root, fam, size) != CHUNK_OK || chunk_child(&root, 0, &terrain) != CHUNK_OK || terrain.kind != CHUNK_KIND_HEADER) {
        exchange_fail(err, "not a track art file (FAM)");
        return 0;
    }
    if (terrain.count < TRACK_GENERIC_BANKS) {
        /* short: track new puts the file's name in front, in the same 200 bytes */
        exchange_fail(err, "%u terrain banks, %d needed (0-3 road, shoulder, verge, grass: letter A; 10 the fence: letter C); docs/modding.md, Art for a generic track",
                      terrain.count, (int)TRACK_GENERIC_BANKS);
        return 0;
    }
    const char fence[4] = { 'C', '0', '0', '0' };
    if (chunk_child(&terrain, TRACK_GENERIC_BANKS - 1, &bank) != CHUNK_OK || bank.kind != CHUNK_KIND_BITMAP ||
        chunk_bitmaps(&bank, &fsh) != FSH_OK || fsh_find(&fsh, fence, &at) != FSH_OK) {
        exchange_fail(err, "terrain bank %d has no letter C, the fence's texture (docs/modding.md, Art for a generic track)", TRACK_GENERIC_BANKS - 1);
        return 0;
    }
    return 1;
}
