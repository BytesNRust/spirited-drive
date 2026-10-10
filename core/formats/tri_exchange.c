#include "formats/tri_exchange.h"

#include "formats/table_exchange.h"
#include "formats/tri.h"

#include <stdlib.h>
#include <string.h>

enum {
    NODES_OFFSET = 0x98C,
    NODE_SIZE = 36,
    BRIGHTNESS_OFFSET = 0x15B0C,
    BRIGHTNESS_SIZE = 3
};

/* The header scalars of 0043e4e0; segment_count, terrain_size and descriptor_count are derived on write. */
static const RecordField track_fields[] = {
    { 0x00, REC_U8, 1, "version" },
    { 0x02, REC_U16, 1, "unknown_0002" },
    { 0x04, REC_U16, 1, "circuit_segments" },
    { 0x06, REC_U16, 1, "segment_count" },
    { 0x08, REC_U16, 1, "unknown_0008" },
    { 0x0a, REC_U16, 1, "unknown_000a" },
    { 0x0c, REC_I32, 3, "unknown_000c" },
    { 0x18, REC_U16, 1, "node0_slope" },
    { 0x1a, REC_U16, 3, "unknown_001a" },
    { 0x20, REC_U16, 1, "node0_orientation" },
    { 0x22, REC_U16, 1, "unknown_0022" },
    { 0x24, REC_U32, 1, "terrain_size" },
    { 0x28, REC_U32, 1, "unknown_0028" },
    { 0x16214, REC_U32, 1, "descriptor_count" },
    { 0x16218, REC_U32, 1, "scenery_slots" }
};

static const RecordField node_fields[] = {
    { 0x00, REC_U8, 1, "left_verge" },
    { 0x01, REC_U8, 1, "right_verge" },
    { 0x02, REC_U8, 1, "left_barrier" },
    { 0x03, REC_U8, 1, "right_barrier" },
    { 0x04, REC_BYTES, 3, "flags" },
    { 0x07, REC_U8, 1, "property" },
    { 0x08, REC_I32, 1, "x" },
    { 0x0c, REC_I32, 1, "y" },
    { 0x10, REC_I32, 1, "z" },
    { 0x14, REC_I16, 1, "slope" },
    { 0x16, REC_I16, 1, "slant_a" },
    { 0x18, REC_I16, 1, "orientation" },
    { 0x1a, REC_U16, 1, "unknown_1a" },
    { 0x1c, REC_I16, 1, "z_orientation" },
    { 0x1e, REC_I16, 1, "slant_b" },
    { 0x20, REC_I16, 1, "x_orientation" },
    { 0x22, REC_U16, 1, "unknown_22" }
};

static const RecordField brightness_fields[] = {
    { 0, REC_U8, 1, "brightness" },
    { 1, REC_U8, 1, "unknown_1" },
    { 2, REC_U8, 1, "unknown_2" }
};

static const RecordField descriptor_fields[] = {
    { 0x00, REC_U8, 1, "flags" },
    { 0x01, REC_U8, 1, "type" },
    { 0x02, REC_U8, 1, "resource_id" },
    { 0x03, REC_U8, 1, "resource2_id" },
    { 0x04, REC_U16, 1, "unknown_04" },
    { 0x06, REC_I16, 1, "width" },
    { 0x08, REC_U16, 1, "unknown_08" },
    { 0x0a, REC_U16, 1, "unknown_0a" },
    { 0x0c, REC_U16, 1, "unknown_0c" },
    { 0x0e, REC_I16, 1, "height" }
};

static const RecordField scenery_fields[] = {
    { 0x00, REC_I32, 1, "node" },
    { 0x04, REC_U8, 1, "descriptor" },
    { 0x05, REC_U8, 1, "orientation" },
    { 0x06, REC_BYTES, 4, "flags" },
    { 0x0a, REC_I16, 1, "x" },
    { 0x0c, REC_I16, 1, "y" },
    { 0x0e, REC_I16, 1, "z" }
};

/* A TRKD chunk: the 12-byte header is rewritten by nobody, so only the payload is a table. */
static const RecordField segment_fields[] = {
    { 0x0c, REC_U8, 1, "unknown_0c" },
    { 0x0d, REC_U8, 1, "fence" },
    { 0x0e, REC_BYTES, 10, "texture" },
    { 0x18, REC_I16, 3, "r0_middle" },
    { 0x1e, REC_I16, 15, "r0_right" },
    { 0x3c, REC_I16, 15, "r0_left" },
    { 0x5a, REC_I16, 3, "r1_middle" },
    { 0x60, REC_I16, 15, "r1_right" },
    { 0x7e, REC_I16, 15, "r1_left" },
    { 0x9c, REC_I16, 3, "r2_middle" },
    { 0xa2, REC_I16, 15, "r2_right" },
    { 0xc0, REC_I16, 15, "r2_left" },
    { 0xde, REC_I16, 3, "r3_middle" },
    { 0xe4, REC_I16, 15, "r3_right" },
    { 0x102, REC_I16, 15, "r3_left" },
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

int tri_export(const uint8_t *data, size_t size, FileSet *out, ExchangeError *err)
{
    Tri tri;
    const TriResult r = tri_open(&tri, data, size);
    if (r != TRI_OK) { exchange_fail(err, "not a readable TRI: %s", tri_result_str(r)); return 0; }
    const size_t nodes = (size_t)tri.node_count + 1 <= TRI_NODE_CAPACITY ? (size_t)tri.node_count + 1 : TRI_NODE_CAPACITY;
    size_t scenery = 0;
    while (scenery < TRI_SCENERY_MAX && (int32_t)(data[tri.scenery_offset + 16 * scenery] | data[tri.scenery_offset + 16 * scenery + 1] << 8 |
                                                  data[tri.scenery_offset + 16 * scenery + 2] << 16 |
                                                  (uint32_t)data[tri.scenery_offset + 16 * scenery + 3] << 24) != -1) ++scenery;
    return record_export(data, TRI_HEADER_SIZE, track_fields, COUNT(track_fields),
                         "TRI header scalars; segment_count, terrain_size and descriptor_count are derived on pack", out, "track.tsv", err) &&
           table_export(data + NODES_OFFSET, NODE_SIZE, nodes, node_fields, COUNT(node_fields), "road nodes (the last row is the closing node)",
                        out, "nodes.tsv", err) &&
           table_export(data + BRIGHTNESS_OFFSET, BRIGHTNESS_SIZE, tri.segment_count, brightness_fields, COUNT(brightness_fields),
                        "per-segment brightness table", out, "brightness.tsv", err) &&
           table_export(data + tri.descriptors_offset, TRI_DESCRIPTOR_SIZE, tri.descriptor_count, descriptor_fields, COUNT(descriptor_fields),
                        "scenery object descriptors", out, "descriptors.tsv", err) &&
           table_export(data + tri.scenery_offset, TRI_SCENERY_SIZE, scenery, scenery_fields, COUNT(scenery_fields),
                        "scenery placements up to the -1 terminator (at most 1000)", out, "scenery.tsv", err) &&
           table_export(data + tri.terrain_offset + 12, TRI_SEGMENT_SIZE, tri.segment_count, segment_fields, COUNT(segment_fields),
                        "terrain strips: 4 rows of the road centre and 5 points to each side, as deltas", out, "segments.tsv", err);
}

int tri_import(const uint8_t *original, size_t size, const FileSet *in, uint8_t **out, size_t *out_len, ExchangeError *err)
{
    Tri tri;
    const TriResult r = tri_open(&tri, original, size);
    if (r != TRI_OK) { exchange_fail(err, "original is not a readable TRI: %s", tri_result_str(r)); return 0; }
    uint8_t *buf = malloc(size);
    if (buf == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(buf, original, size);
    const size_t nodes = (size_t)tri.node_count + 1 <= TRI_NODE_CAPACITY ? (size_t)tri.node_count + 1 : TRI_NODE_CAPACITY;
    size_t found = 0;
    int ok = record_import(buf, TRI_HEADER_SIZE, track_fields, COUNT(track_fields), in, "track.tsv", err) &&
             table_import(buf + NODES_OFFSET, NODE_SIZE, nodes, nodes, node_fields, COUNT(node_fields), in, "nodes.tsv", &found, err) &&
             table_import(buf + BRIGHTNESS_OFFSET, BRIGHTNESS_SIZE, tri.segment_count, tri.segment_count, brightness_fields,
                          COUNT(brightness_fields), in, "brightness.tsv", &found, err) &&
             table_import(buf + tri.descriptors_offset, TRI_DESCRIPTOR_SIZE, tri.descriptor_count, tri.descriptor_count,
                          descriptor_fields, COUNT(descriptor_fields), in, "descriptors.tsv", &found, err) &&
             table_import(buf + tri.terrain_offset + 12, TRI_SEGMENT_SIZE, tri.segment_count, tri.segment_count, segment_fields,
                          COUNT(segment_fields), in, "segments.tsv", &found, err);
    if (ok) {   /* placements: any count up to the slots, then the terminator */
        ok = table_import(buf + tri.scenery_offset, TRI_SCENERY_SIZE, 0, TRI_SCENERY_MAX - 1, scenery_fields, COUNT(scenery_fields), in,
                          "scenery.tsv", &found, err);
        if (ok) {
            uint8_t *terminator = buf + tri.scenery_offset + TRI_SCENERY_SIZE * found;
            memset(terminator, 0xff, 4);
        }
    }
    if (ok) {
        Tri edited;
        TriParts parts;
        const TriResult e = tri_open(&edited, buf, size);
        if (e != TRI_OK || tri_parts(&edited, &parts) != TRI_OK) {
            exchange_fail(err, "the edited tables do not make a valid TRI: %s", tri_result_str(e));
            ok = 0;
        } else {
            uint8_t *file = malloc(tri_write_size(&parts));
            if (file == NULL || tri_write(&parts, file, tri_write_size(&parts), out_len) != TRI_OK) {
                free(file);
                exchange_fail(err, "cannot assemble the TRI");
                ok = 0;
            } else {
                *out = file;
            }
        }
    }
    free(buf);
    return ok;
}
