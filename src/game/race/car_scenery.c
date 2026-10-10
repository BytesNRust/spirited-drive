#include "game/race/car_scenery.h"

enum { SCENERY_RECORD = 0x10, SCENERY_END = -1 };

/* The node word of record `index`; the original reads it unchecked. */
static int32_t record_node(const Tri *tri, int32_t index)
{
    const size_t at = tri->scenery_offset + (size_t)index * SCENERY_RECORD;
    if (index < 0 || at + 4 > tri->size) return SCENERY_END;  /* port guard */
    const uint8_t *p = tri->data + at;
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

int32_t car_scenery_at(Car *car, const Tri *tri, int32_t node)
{
    if (node < car->scenery_node || record_node(tri, car->scenery_index + 1) == SCENERY_END) {
        if (node < car->scenery_node) {
            while (car->scenery_index != 0) {
                if (node == car->scenery_node) return 1;
                car->scenery_index--;
                car->scenery_node = record_node(tri, car->scenery_index);
                if (car->scenery_node < node) return 0;
            }
        }
        return 0;
    }
    do {
        if (node == car->scenery_node) return 1;
        car->scenery_index++;
        car->scenery_node = record_node(tri, car->scenery_index);
    } while (car->scenery_node <= node && record_node(tri, car->scenery_index + 1) != SCENERY_END);
    return 0;
}
