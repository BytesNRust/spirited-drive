#include "game/frontend/showcase_variants.h"

#include "game/race/car_load.h"
#include "game/race/race_models.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"
#include "game/viewer/drive_livery.h"

/* ----- the PS1 livery: its files are the user's own, so the entry reads them once for a car and keeps what it found ---- */

typedef struct {
    int model, offered, chosen;
    uint16_t colours[2];    /* the main body colour (BGR555) of the normal livery and of the PS1's */
} Ps1Car;

static Ps1Car ps1 = { -1, 0, 0, { 0, 0 } };

static const Ps1Car *ps1_car(int model)
{
    if (ps1.model == model) return &ps1;
    ps1 = (Ps1Car){ model, 0, 0, { 0, 0 } };
    if (model < 0 || model >= CAR_MODELS || port_extras_load().psx_liveries == 0) return &ps1;
    const PortSettings settings = port_settings_load();
    if (settings.psx_dir[0] == '\0') return &ps1;
    ps1.offered = drive_livery_swatch(settings.psx_dir, car_model_names[model], ps1.colours);
    ps1.chosen = ps1.offered != 0 && port_extras_psx_livery_car(model) != 0;
    return &ps1;
}

static int ps1_offered(int model) { return ps1_car(model)->offered; }
static int ps1_chosen(int model) { return ps1_car(model)->chosen; }

static void ps1_apply(int model, int on)
{
    port_extras_set_psx_livery_car(model, on);
    ps1_car(model);
    ps1.chosen = on != 0;
}

/* BGR555 to the screen's RGB, the 5-bit channels widened to 8 */
static int32_t rgb_of(uint16_t bgr)
{
    const int32_t r = bgr & 31, g = (bgr >> 5) & 31, b = (bgr >> 10) & 31;
    return ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
}

static void ps1_draw(const CarVariantCanvas *c, int model, int on)
{
    const Ps1Car *car = ps1_car(model);
    c->fill(c->ctx, c->x0, c->y0, c->x1, c->y1, rgb_of(car->colours[on != 0]));
}

const CarVariant showcase_variants[] = {
    { "ps1_livery", CAR_VARIANT_COLOUR, ps1_offered, ps1_chosen, ps1_apply, ps1_draw },
};
const int showcase_variant_count = (int)(sizeof showcase_variants / sizeof showcase_variants[0]);

void showcase_variants_reset(void) { ps1.model = -1; }
