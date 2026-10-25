/* port: the PS1 look's baked light (re/notes/psx-lgt-format.md, branch docs/psx-lgt). The PS1 CD has, for each track and time
 * of day (A, B, C), a Z<TRACK><A|B|C>.LGT: u32 node count, u32 scale, 11 bytes of intensity a node (0 black, 255 full)
 * in the order of the node's terrain row (TriRow: middle, right[0..4], left[0..4]), then a 12-byte CRCF trailer. The road's
 * points never darken; the terrain under a bridge and the walls of a tunnel do. The bytes are the user's own files, read
 * at the race's start; nothing of them is in the repo, and the tests use made-up rows. */
#ifndef TNFS_PSX_LIGHT_H
#define TNFS_PSX_LIGHT_H

#include <stddef.h>
#include <stdint.h>

enum { PSX_LIGHT_POINTS = 11, PSX_LIGHT_FULL = 255 };

typedef struct {
    uint8_t *data;      /* nodes * PSX_LIGHT_POINTS bytes (owned), NULL when none is loaded */
    uint32_t nodes;
} PsxLight;

/* The file's bytes as a PsxLight: its node count must be `expected_nodes` (the track's ring nodes) and the file at least as
 * long as the rows. 1 on success; `out` is untouched otherwise. */
int psx_light_parse(const uint8_t *file, size_t size, uint32_t expected_nodes, PsxLight *out);

void psx_light_free(PsxLight *light);

/* The intensity of terrain-row point `point` (0..10, TriRow order) of node `node` (taken modulo the nodes); the file's last
 * node is not light (random bytes) and takes the one before it; PSX_LIGHT_FULL without data or for a point out of range. */
uint8_t psx_light_at(const PsxLight *light, uint32_t node, int point);

/* The light of what stands at a node (a scenery object): the mean of its terrain points' intensities, the road's left out;
 * PSX_LIGHT_FULL without data. */
uint8_t psx_light_node(const PsxLight *light, uint32_t node);

#endif
