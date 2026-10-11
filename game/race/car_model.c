#include "game/race/car_model.h"

enum { SIZE_SHIFT = 9 };

void car_model_size(Car *car, const int32_t (*points)[3], int32_t count)
{
    int32_t least[3] = { 0, 0, 0 }, most[3] = { 0, 0, 0 };
    for (int32_t i = 0; i < count; i++)
        for (int k = 0; k < 3; k++) {
            if (points[i][k] < least[k]) least[k] = points[i][k];
            if (points[i][k] > most[k]) most[k] = points[i][k];
        }
    car->model_width = (int32_t)(((uint32_t)most[0] - (uint32_t)least[0]) << SIZE_SHIFT);
    car->model_height = (int32_t)(((uint32_t)most[1] - (uint32_t)least[1]) << SIZE_SHIFT);
    car->model_length = (int32_t)(((uint32_t)most[2] - (uint32_t)least[2]) << SIZE_SHIFT);
}
