/* port: where a car's rear wheels are and which way it heads, for the spray (spray_v4) and the rally's dust trail: the heading of a car drawn by its yaw or by its matrix, the pace of a car
 * whose physics does not run, and the two rear wheels. Pure: no drawing, no globals. */
#ifndef TNFS_RAIN_SPRAY_H
#define TNFS_RAIN_SPRAY_H

#include <stdint.h>

/* The car's heading as a unit vector (x, z; 16.16) for its wheels: its yaw (16.16 turn, the high bits: `yaw >> 8`), or, for a car that is drawn with its matrix (use_matrix: the rivals' and the
 * traffic's, whose yaw stays 0), the matrix's forward (`matrix6`, `matrix8`: its x and z). The matrix's is normalised; a degenerate one falls back to the yaw. */
void rain_spray_heading(int use_matrix, int32_t yaw, int32_t matrix6, int32_t matrix8, int32_t forward[2]);

/* The pace the spray goes by (16.16 m/s). A car whose physics runs has its `speed` (0x070, 00430cb0); the traffic's and the police's never does: the road's driver moves it by its velocity and `speed`
 * stays 0, so no wisp was ever dropped for it. A zero `speed` falls back to 00430cb0's own formula on the world velocity (`vx`, `vz`, 16.16): the larger component and a quarter of the smaller. */
int32_t rain_spray_speed(int32_t speed, int32_t vx, int32_t vz);

/* Where a car's rear wheels are and which way is its right: `at` (x, y, z, 16.16 m) the car's centre, `forward` its heading (rain_spray_heading). wheel[0] the left, wheel[1] the right
 * (1.4 m behind the centre, 0.8 m out, 0.3 m down); `right` the car's right (x, z; 16.16 unit). */
void rain_spray_wheels(const int32_t at[3], const int32_t forward[2], int32_t wheel[2][3], int32_t right[2]);

#endif
