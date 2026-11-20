/* A car's body matrix from its angles (NFS_WIN.EXE 004591d4): z about x, then y, the angles as the race keeps them (24 bits a turn: the builders take
 * them >> 14 as 10-bit angles). The race's car draw and the Studio's 3D view both orient a car through this. */
#ifndef TNFS_CAR_POSE_H
#define TNFS_CAR_POSE_H

#include <stdint.h>

/* 00491e60, 00491f30, 00491ec8 and 004591d4: the 16.16 matrix of a car with `heading` about y, `angle_x` about x and `angle_z` about z */
void car_body_matrix(int32_t heading, int32_t angle_x, int32_t angle_z, int32_t out[9]);

#endif
