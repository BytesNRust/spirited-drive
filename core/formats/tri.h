/* TRI: track geometry (the .TRI files in simdata/misc), loaded by 0043e4e0 into the globals
 * at 005041d0..00509788. All integers little-endian. The parser keeps a pointer
 * into the caller's buffer and decodes records on demand; nothing is allocated
 * and no value is converted: fixed-point fields are returned as stored.
 *
 *   off      size   meaning                                     confidence
 *   0x00000  1      version, always 0x11                        high (all 17 files)
 *   0x00002  2      unknown_0002                                low
 *   0x00004  2      circuit_segments: 0 = point-to-point stage, high (0043e4e0)
 *                   else ring length in segments (power of 2)
 *   0x00006  2      segment_count: TRKD chunks, nodes = x 4     high (0043e4e0, data)
 *   0x00008  2      unknown_0008                                low
 *   0x0000a  2      unknown_000a (6 in every file)              low
 *   0x0000c  12     3 x i32 unknown_000c/0010/0014              low
 *   0x00018  2      node0_slope: copied over node 0 slope       high (0043e4e0)
 *   0x0001a  6      unknown_001a/001c/001e                      low
 *   0x00020  2      node0_orientation: copied over node 0       high (0043e4e0)
 *   0x00022  2      unknown_0022                                low
 *   0x00024  4      terrain_size: bytes of the TRKD block       high (0043e4e0, data)
 *   0x00028  4      unknown_0028 (0x2d..0x3f)                   low
 *   0x0002c  2400   segment offset table, 600 x u32: byte       high (0043e244, data)
 *                   offset of chunk i inside the terrain block
 *   0x0098c  86400  2400 nodes x 36 bytes (see TriNode)         high
 *   0x15b0c  1800   600 x 3 bytes, indexed by segment; byte 0   medium (0043e4e0)
 *                   of entries 7..segment_count-1 is brightness
 *                   scaled at load time; bytes 1-2 unknown
 *   0x16214  4      descriptor_count (64)                       high (0043e4e0)
 *   0x16218  4      scenery_slots (1000); 0040d6a0 allocates a  high
 *                   2-byte runtime state per slot
 *   0x1621c  12     chunk header: "SJBO", u32 chunk_size, u32 0 high (0043e4e0)
 *                   chunk_size counts the header; the next
 *                   block starts at 0x1621c + chunk_size
 *   0x16228  16 x n descriptors (see TriDescriptor)             medium (reference)
 *            16 x m scenery records (see TriScenery), ended by
 *            a record whose first i32 is -1, at most 1000      high (0043e4e0 loop)
 *   +chunk   terrain_size bytes: segment_count x 288-byte TRKD chunks (see TriSegment)
 *            optionally followed by a trailer (AL22.TRI has a 12-byte "CRCF" one)
 *
 * Whole-file layout beyond the loader's reads (field meanings of nodes, rows,
 * descriptors and scenery) was cross-checked against the community OpenNFS1
 * notes; only the layout, not its code, was used. */
#ifndef TNFS_TRI_H
#define TNFS_TRI_H

#include <stddef.h>
#include <stdint.h>

enum {
    TRI_VERSION = 0x11,
    TRI_NODE_CAPACITY = 2400,    /* fixed node array in the header block */
    TRI_SEGMENT_CAPACITY = 600,  /* offset table and brightness table size */
    TRI_NODES_PER_SEGMENT = 4,   /* one TRKD chunk holds the 4 rows of 4 nodes */
    TRI_SCENERY_MAX = 1000,      /* 16000 bytes scanned by 0043e4e0 */
    TRI_OPEN_RING_SEGMENTS = 0x8000, /* ring size 0043e4e0 uses when circuit_segments is 0 */
    TRI_HEADER_SIZE = 0x1621C,   /* bytes 0043e4e0 reads before the SJBO chunk */
    TRI_SEGMENT_SIZE = 288,
    TRI_DESCRIPTOR_SIZE = 16,
    TRI_SCENERY_SIZE = 16
};

typedef enum {
    TRI_OK = 0,
    TRI_ERR_HEADER,  /* too short, wrong version or impossible counts */
    TRI_ERR_BOUNDS,  /* block, chunk or index outside the data */
    TRI_ERR_CHUNK    /* chunk tag is not the expected one */
} TriResult;

/* Fixed-point position: 3 x i32 at node offset 8. Consecutive nodes advance
 * about 0x60000 along z, consistent with 16.16; the exact scale is unconfirmed. */
typedef struct {
    int32_t x, y, z;
} TriVec32;

/* 36-byte virtual road node at 0x98c + 36 * i. Index segment_count * 4 holds one
 * extra node past the end (closing node on circuits); it is readable. */
typedef struct {
    uint8_t left_verge;        /* +0, distance to road edge; reference, medium */
    uint8_t right_verge;       /* +1 */
    uint8_t left_barrier;      /* +2 */
    uint8_t right_barrier;     /* +3 */
    uint8_t flags[3];          /* +4..+6, unknown; 0x11 0x11 0x22 on AL1 */
    uint8_t property;          /* +7, road property (tunnel, surface sound); reference, medium */
    TriVec32 position;         /* +8 */
    int16_t slope;             /* +0x14, 14-bit angle: values above 0x2000 are negative via -0x3fff */
    int16_t slant_a;           /* +0x16 */
    int16_t orientation;       /* +0x18, heading, 0x4000 = full turn (0x1000 = right) */
    uint16_t unknown_1a;       /* +0x1a */
    int16_t z_orientation;     /* +0x1c, reference, low confidence */
    int16_t slant_b;           /* +0x1e, reference reads it as the final slant */
    int16_t x_orientation;     /* +0x20 */
    uint16_t unknown_22;       /* +0x22 */
} TriNode;

/* 16-byte scenery object descriptor (after the SJBO chunk header). Layout from
 * the reference; the loader only counts them. */
typedef struct {
    uint8_t flags;             /* +0, bit 2 = animated */
    uint8_t type;              /* +1, 1 model, 4 bitmap, 6 two-sided bitmap */
    uint8_t resource_id;       /* +2 */
    uint8_t resource2_id;      /* +3, used by type 6 */
    uint16_t unknown_04;       /* +4 */
    int16_t width;             /* +6, raw units */
    uint16_t unknown_08;       /* +8, low byte is the frame count when animated */
    uint16_t unknown_0a;       /* +0xa */
    uint16_t unknown_0c;       /* +0xc */
    int16_t height;            /* +0xe, raw units */
} TriDescriptor;

/* 16-byte scenery placement. */
typedef struct {
    int32_t node;              /* +0, road node; may exceed the node count (wraps) */
    uint8_t descriptor;        /* +4, index into the descriptors */
    uint8_t orientation;       /* +5 */
    uint8_t flags[4];          /* +6, unknown */
    int16_t x, y, z;           /* +0xa, offset from the node, raw units */
} TriScenery;

/* Terrain point; rows store deltas, each from the previous point of its chain. */
typedef struct {
    int16_t x, y, z;
} TriPoint;

/* One terrain row: the road centre and the 5 points outward on each side. */
typedef struct {
    TriPoint middle;
    TriPoint right[5];         /* right[0] is relative to middle */
    TriPoint left[5];          /* left[0] is relative to middle */
} TriRow;

/* One 288-byte "TRKD" chunk: terrain strip for 4 consecutive nodes. */
typedef struct {
    uint32_t length;           /* +4, payload size after the 12-byte header (276) */
    uint32_t unknown_08;       /* +8, 0 in every file */
    uint8_t unknown_0c;        /* +0xc */
    uint8_t fence;             /* +0xd, bit 7 left fence, bit 6 right fence, low 6 bits texture */
    uint8_t texture[10];       /* +0xe, texture ids */
    TriRow rows[4];            /* +0x18, 66 bytes each, one per node */
} TriSegment;

enum {
    TRI_FENCE_LEFT = 0x80,
    TRI_FENCE_RIGHT = 0x40,
    TRI_FENCE_TEXTURE_MASK = 0x3F
};

typedef struct {
    const uint8_t *data;
    size_t size;
    uint16_t circuit_segments;      /* +4 */
    uint16_t segment_count;         /* +6 */
    uint16_t node0_slope;           /* +0x18 */
    uint16_t node0_orientation;     /* +0x20 */
    uint32_t terrain_size;          /* +0x24 */
    uint32_t descriptor_count;      /* +0x16214 */
    uint32_t scenery_slots;         /* +0x16218 */
    uint32_t scenery_count;         /* records before the -1 terminator, at most 1000 */
    uint32_t node_count;            /* segment_count * 4 */
    uint32_t ring_segments;         /* circuit_segments, or 0x8000 for a stage (0043e4e0) */
    uint32_t ring_nodes;            /* ring_segments * 4 */
    size_t descriptors_offset;      /* file offsets of the variable blocks */
    size_t scenery_offset;
    size_t terrain_offset;
    size_t trailer_size;            /* bytes after the terrain block */
} Tri;

/* Keeps a pointer to data; it must outlive the Tri. Validates the header, the
 * object chunk, the terrain block and every segment chunk. */
TriResult tri_open(Tri *tri, const uint8_t *data, size_t size);

/* A point-to-point stage has circuit_segments == 0 (Alpine, City, Coastal). */
int tri_is_circuit(const Tri *tri);

/* index <= node_count (the extra closing node), < TRI_NODE_CAPACITY. */
TriResult tri_node(const Tri *tri, uint32_t index, TriNode *out);
TriResult tri_descriptor(const Tri *tri, uint32_t index, TriDescriptor *out);
TriResult tri_scenery(const Tri *tri, uint32_t index, TriScenery *out);
TriResult tri_segment(const Tri *tri, uint32_t index, TriSegment *out);

/* Raw 3-byte entry of the per-segment table at 0x15b0c. */
TriResult tri_brightness(const Tri *tri, uint32_t index, uint8_t out[3]);

/* The load-time scaling of 0043e4e0: value * scale (16.16), rounded. */
uint8_t tri_brightness_scale(uint8_t value, uint32_t scale16);

/* The pieces of a file, for rebuilding one. The writer recomputes what depends on the sizes
 * (see tri_write) and copies everything else, so a file taken apart by tri_parts and written
 * back is identical. */
typedef struct {
    const uint8_t *header;       /* TRI_HEADER_SIZE bytes: nodes, brightness table, ... */
    const uint8_t *descriptors;  /* descriptor_count x TRI_DESCRIPTOR_SIZE */
    uint32_t descriptor_count;
    const uint8_t *scenery;      /* TRI_SCENERY_MAX x TRI_SCENERY_SIZE: the records, the -1 terminator, the padding */
    const uint8_t *terrain;      /* segment_count x TRI_SEGMENT_SIZE */
    uint32_t segment_count;
    const uint8_t *trailer;      /* e.g. AL22.TRI's 12-byte "CRCF" */
    size_t trailer_size;
} TriParts;

/* Views into an opened file; TRI_ERR_BOUNDS when the object chunk is not the usual
 * descriptors + 1000 placements. */
TriResult tri_parts(const Tri *tri, TriParts *parts);

size_t tri_write_size(const TriParts *parts);

/* Writes the file. Rewritten from the parts: segment count (+6), terrain size (+0x24), the
 * 600 segment offsets (288 x i, as in all 17 CD files, 0043e244), descriptor count
 * (+0x16214) and the SJBO chunk's size. TRI_ERR_BOUNDS when a count is over its capacity
 * (600 segments) or dst is too small. */
TriResult tri_write(const TriParts *parts, uint8_t *dst, size_t cap, size_t *len);

const char *tri_result_str(TriResult result);

#endif
