/* port: the car variants the car showcase's box steps through (the PS1 livery first; racing decals and stickers later).
 * A variant is a table entry; the box shows one choice at a time: the car as it is, or one of the variants the car is
 * offered. re/notes/car-variants.md says how to add one. */
#ifndef TNFS_CAR_VARIANT_H
#define TNFS_CAR_VARIANT_H

#include <stdint.h>

typedef enum { CAR_VARIANT_COLOUR, CAR_VARIANT_DECAL } CarVariantKind;

/* where a variant draws its preview: the box's inside, and the one primitive it paints with */
typedef struct {
    void *ctx;
    void (*fill)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t rgb);   /* rgb 0xRRGGBB, both corners in */
    int32_t x0, y0, x1, y1;
} CarVariantCanvas;

typedef struct {
    const char *id;                 /* the name the choice is kept under */
    CarVariantKind kind;
    int (*offered)(int model);      /* 1 when the car (its race model) has this variant */
    int (*chosen)(int model);       /* 1 when the player chose it for the car */
    void (*apply)(int model, int on);   /* keeps the choice; the race reads it from there */
    /* the preview: the car as it is (`on` 0) or with the variant (`on` 1); a colour variant fills the canvas */
    void (*draw)(const CarVariantCanvas *canvas, int model, int on);
} CarVariant;

/* the box's choices for the car: 0 when no variant is offered (no box), else the car as it is and each offered variant */
int car_variant_choices(const CarVariant *list, int count, int model);

/* the choice now: 0 the car as it is, k the k-th offered variant. When more than one is chosen the first wins. */
int car_variant_current(const CarVariant *list, int count, int model);

/* the next (`step` 1) or previous (`step` -1) choice, wrapping, kept through the variants' apply hooks; returns it */
int car_variant_cycle(const CarVariant *list, int count, int model, int step);

/* draws the current choice's preview; returns 0 when the car has no variants (nothing drawn) */
int car_variant_draw(const CarVariant *list, int count, int model, const CarVariantCanvas *canvas);

#endif
