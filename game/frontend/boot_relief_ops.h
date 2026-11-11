/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the image operations the boot screen's red relief is blended with (boot_relief.h), on w x h planes of doubles, row by row. Edges are
 * repeated (a box blur near the border averages the edge pixel again), as the reference the owner approved (i030 out7) was made. */
#ifndef TNFS_BOOT_RELIEF_OPS_H
#define TNFS_BOOT_RELIEF_OPS_H

#include <stdint.h>

typedef struct {
    int w, h;
} Plane;

/* out = the mean of x over the (2r+1)^2 square round each pixel. out may be x. 0 when there is no memory. */
int ops_blur(Plane p, const double *x, int r, double *out);

/* out = the mean of x over the pixels of the square where w is set (w: 0/1 per pixel); where none is, 0. */
int ops_nblur(Plane p, const double *x, const uint8_t *w, int r, double *out);

/* out = 1 where any pixel of the (2r+1)^2 square round it is set. out may not be m. */
void ops_grow(Plane p, const uint8_t *m, int r, uint8_t *out);

/* x along each row (horizontal) or column (vertical): the mean of the 2r+1 pixels round it. out may be x. */
int ops_hbox(Plane p, const double *x, int r, double *out);
int ops_vbox(Plane p, const double *x, int r, double *out);

/* x inside `mask` replaced by the smooth surface its edge gives: the mean outside to start, 800 Jacobi steps on 8 x 8 blocks (a block is in when
 * more than half of it is), up again, then 400 Jacobi steps at full size. w and h are multiples of 8; there is a pixel outside the mask. */
int ops_membrane(Plane p, double *x, const uint8_t *mask);

/* Solves (lambda - laplacian) u = f with the edges closed (Neumann): the 5-point laplacian, each missing neighbour left out. u holds the start. */
int ops_screened_poisson(Plane p, double lambda, const double *f, double *u);

/* The grain: xorshift32 from `seed`, a sample the sum of four uniforms ((x >> 8) / 2^24) minus 2, times sqrt 3 (mean 0, deviation 1). */
void ops_noise(uint32_t seed, double *out, int n);

#endif
