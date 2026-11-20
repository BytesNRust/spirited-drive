/* A car's lights in the palette (car_lights.h). */
#include "game/race/car_lights.h"

#include "game/gfx/x86_int.h"
#include "game/race/race_palette.h"
#include "game/race/car_registry.h"

enum { BRAKE_LIT = 10, LIT_ROWS = 13, POLICE_SLOT = 2, POLICE_FLAGS = 0xc, FLASH_TICKS = 15, POLICE_FIRST = 0xfa,
       POLICE_COLOURS = 4 };

void car_lights(const CarLightOps *ops, const CarLightInputs *in, const Car *car)
{
    const int32_t slot = car->index;
    if (in->notice[0] == 0 && in->notice[1] == 0) {
        const uint8_t entry = in->slot_colours[slot];
        if (entry == 0 || slot >= in->finish_cars) return;
        const int32_t lamp = car_registry_lamp_model(car->model);   /* port (dep59 3b): an added car's are the standard red */
        const int32_t row = lamp + (car->brake > BRAKE_LIT ? LIT_ROWS : 0);
        if (lamp < 0 || row >= RACE_PALETTE_MODEL_LIGHTS) return;   /* port guard: the original reads past the table */
        ops->write(ops->ctx, entry, 1, car_brake_lamp_rows + 3 * row);
        return;
    }
    if (slot != POLICE_SLOT || !(car->flags_176[0] & POLICE_FLAGS)) return;
    if (x_sub(in->tick, *in->flash_tick) > FLASH_TICKS) {
        *in->flash = 1 - *in->flash;
        *in->flash_tick = in->tick;
    }
    ops->write(ops->ctx, POLICE_FIRST, POLICE_COLOURS, race_palette_police_light + (*in->flash != 0 ? 0 : 12));
}
