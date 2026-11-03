/* port: a circuit driven the other way round, made from the file the CD holds. The result is an ordinary TRI whose
 * nodes run the other way, so the ring, the AI, the grid and the lap count see nothing new.
 *
 * New node j is old node (N - j) mod N (N the ring's nodes): node 0 stays the start line, and a segment's four strips
 * (and the one texture row and fence byte they share) stay together, as old segment S - 1 - t is new segment t. What
 * turns with the direction:
 *   node   left and right swap (verges, barriers, the nibbles of the three flag bytes); the heading turns half a
 *          turn; slope and slant (the 14-bit angles) and the sideways vector are negated; the police spot's side
 *          bit flips. The position stays.
 *   rows   the two chains of five swap; texture slot s takes the old slot (s + 5) mod 10; the fence bits swap.
 *   scenery  the node is remapped and the orientation turned half a turn (the offsets are world offsets and stay);
 *          the records are kept in ascending node order, as the walk needs them.
 * Applied twice it gives the file back. */
#ifndef TNFS_TRI_REVERSE_H
#define TNFS_TRI_REVERSE_H

#include "formats/tri.h"

/* Writes into `out` (`size` bytes, the same as `in`) the reversed file. TRI_ERR_HEADER for a file that is not a
 * circuit, the errors of tri_open for one that does not open. */
TriResult tri_reverse(const uint8_t *in, size_t size, uint8_t *out);

/* The centre of a 3D model's footprint in the model's own units (1/16 m), x and z: the models' origin is a corner of
 * them (a tunnel's portal runs 0..384 across), so turning one half a turn about its origin throws it to the other side
 * of the road. */
typedef struct {
    int32_t x, z;
} TriPivot;

/* What tri_reverse_with does beyond tri_reverse:
 *   strip_property  the node's property byte (+7: tunnel, LOD, sound) is that of the strip from the node to the next one, as the
 *                   slope and the heading are, so it comes from the old node before (the new node's next). Left on the node
 *                   itself the tunnels, and what the road shows at their ends, are a strip off: the mouths come out as
 *                   slivers across the opening.
 *   pivots          `pivots[r]` is the centre of the model of resource id r (`pivot_count` of them); a model past the table,
 *                   and the boards, are left as tri_reverse leaves them. The centre of every model stays where it stood and
 *                   only its facing turns. */
typedef struct {
    int strip_property;
    const TriPivot *pivots;
    uint32_t pivot_count;
} TriReverseOptions;

TriResult tri_reverse_with(const uint8_t *in, size_t size, uint8_t *out, const TriReverseOptions *options);

#endif
