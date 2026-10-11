/* The race's results (race_results.h). */
#include "game/race/race_results.h"

enum { SHORT_NAME_ORDER_SIZE = 16 };

/* 00426640: the short-name index (004c5c84: RX-7, NSX, SUPRA, 911, ZR-1,
 * VIPER, 512TR, DIABLO) of each car model, then 8..15 as themselves. */
static const int16_t SHORT_NAME_ORDER[SHORT_NAME_ORDER_SIZE] = { 2, 7, 3, 4, 6, 5, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15 };

int32_t race_short_name_index(int16_t model)
{
    /* cwde, then ax from the table: the high half is the model's sign */
    const uint32_t high = model < 0 ? 0xffff0000u : 0;
    const int16_t index = model >= 0 && model < SHORT_NAME_ORDER_SIZE ? SHORT_NAME_ORDER[model] : 0;  /* port guard: the original reads its stack past the table */
    return (int32_t)(high | (uint16_t)index);
}
