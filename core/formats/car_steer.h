/* A front wheel's steering (dep59 tools B2): its turn at full lock, fitted on a model, and the offsets of a new wheel generated
 * from an angle. A turn part lists the wheel's vertices and, from its first offset vertex on, the offset each one moves by at
 * full lock (docs/modding/car-model.md, "Section A"): the race adds them scaled by the steering. The turn is about a vertical
 * axis (y); in x and z the offsets fit a rotation to within 1.9 units on every wheel of the CD. Degrees count from +x towards
 * +z, so a positive angle turns the wheel's front (+z) towards -x, the car's left. */
#ifndef TNFS_CAR_STEER_H
#define TNFS_CAR_STEER_H

#include "formats/exchange.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"

#include <stdint.h>

typedef struct {
    double degrees;            /* the turn at full lock */
    double pivot_x, pivot_z;   /* the vertical axis it turns about */
    double residual;           /* the largest distance in x z between a vertex turned so and where its offset puts it */
} CarSteer;

/* the turn part `part` of `desc` makes at full lock, a least-squares fit over its vertices and offsets; 0 with the reason in `err`
 * (no such part, fewer than 2 vertices, an index outside the vertices, no turn) */
int car_steer_fit(const MeshDesc *desc, uint32_t part, CarSteer *out, ExchangeError *err);

/* the offsets of `count` wheel vertices turned `degrees` about a vertical axis through the wheel's outer side halfway along it:
 * `outer` -1 for the left wheel (its least x), +1 for the right (its most x); z the middle of its extent. `offsets` gets `count`
 * vertices, y 0, rounded. 0 with the reason in `err` (no vertex, `outer` not -1 or +1) */
int car_steer_offsets(const MeshVertex *wheel, uint32_t count, double degrees, int outer, MeshVertex *offsets, ExchangeError *err);

#endif
