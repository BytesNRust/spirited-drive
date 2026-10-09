/* The car's collision body and the computer driver's tables, from its
 * CARFAMS/<name>.PDN file (0040fa3c, 004407b8). */
#ifndef TNFS_CAR_BODY_H
#define TNFS_CAR_BODY_H

#include "game/race/car.h"

#include <stddef.h>
#include <stdint.h>

enum { CAR_PDN_SIZE = 0x1cc, CAR_PDN_SPEEDS = 100, CAR_PDN_FACTORS = 6 };

/* A PDN file unpacked (00517580 + 4 * slot points to one). */
#pragma pack(push, 1)
typedef struct {
    int32_t extents[3];            /* the body's half sizes */
    int32_t unknown_0c;
    int32_t unknown_10;
    int32_t mass;
    int32_t unknown_18;
    int32_t speeds[CAR_PDN_SPEEDS];   /* +0x1c */
    int32_t factors[CAR_PDN_FACTORS]; /* +0x1ac */
    int32_t unknown_1c4;
    int32_t unknown_1c8;
} CarPdn;
#pragma pack(pop)
typedef char car_pdn_size_check[sizeof(CarPdn) == CAR_PDN_SIZE ? 1 : -1];

/* The QFS-compressed file unpacked; 0 when it does not hold a record. */
int car_pdn_read(CarPdn *pdn, const uint8_t *file, size_t size);

/* 004407b8 (with 0040fa3c and 0041da00): the body from the PDN, and the
 * car's half sizes from it, or from the model's (+0x64a..) when the PDN
 * gives none. */
void car_body_init(Car *car, const CarPdn *pdn);

#endif
