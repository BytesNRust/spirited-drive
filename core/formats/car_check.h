/* What Studio finds wrong in a car before it is built, as a list a panel can show (dep90 items 2 and 9). An error is something the race
 * cannot draw or turn right; a warning is a car that will build but looks wrong. car_check_wheels reads the near mesh's wheels: the two
 * parts the steering turns, their sides and axle, and the quads that carry the tyre frames (docs/modding/car-model.md, "Section A"
 * and "The wheels"). A car with no steered parts (a traffic car) has nothing to check there. */
#ifndef TNFS_CAR_CHECK_H
#define TNFS_CAR_CHECK_H

#include "formats/mesh_build.h"

#include <stddef.h>

typedef enum { CAR_CHECK_ERROR, CAR_CHECK_WARNING } CarCheckSeverity;
typedef struct { CarCheckSeverity severity; char text[200]; } CarCheckItem;
typedef struct { CarCheckItem *item; size_t count, cap; } CarCheckList;

void car_check_free(CarCheckList *list);
/* one finding appended; 0 when out of memory */
int car_check_add(CarCheckList *list, CarCheckSeverity severity, const char *format, ...) __attribute__((format(printf, 3, 4)));
size_t car_check_count(const CarCheckList *list, CarCheckSeverity severity);
/* the wheel findings of `mesh` (the near mesh) appended to `list`; 0 only when out of memory */
int car_check_wheels(const MeshDesc *mesh, CarCheckList *list);

#endif
