#include "game/frontend/showcase_variants.h"

#include "game/race/car_load.h"
#include "game/race/car_paint.h"
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
    ps1.chosen = ps1.offered != 0 && port_extras_car_paint(model) == PAINT_PS1;
    return &ps1;
}

static int ps1_offered(int model) { return ps1_car(model)->offered; }
static int ps1_chosen(int model) { return ps1_car(model)->chosen; }

/* a choice is kept as car_paint's id: this entry's on sets it, its off clears it only when it is the one set, so the entry the
 * box moved to (applied in the list's order, maybe before this one) keeps its own */
static void keep(int model, int id, int on)
{
    if (on) port_extras_set_car_paint(model, id);
    else if (port_extras_car_paint(model) == id) port_extras_set_car_paint(model, PAINT_OWN);
}

static void ps1_apply(int model, int on)
{
    keep(model, PAINT_PS1, on);
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

/* ----- the catalogue paints (car_paint.h): offered with the car colours switch, with or without PS1 files ---- */

static int paint_offered(int model, int k)
{
    return k < car_paint_count(model) && port_extras_load().psx_liveries != 0;
}

static int paint_chosen(int model, int k)
{
    return paint_offered(model, k) && port_extras_car_paint(model) == PAINT_CATALOGUE + k;
}

static void paint_draw(const CarVariantCanvas *c, int model, int k, int on)
{
    const uint8_t *rgb = on ? car_paint(model, k)->rgb : car_paint_default(model);
    if (rgb != NULL) c->fill(c->ctx, c->x0, c->y0, c->x1, c->y1, rgb[0] << 16 | rgb[1] << 8 | rgb[2]);
}

/* one entry for each of the CAR_PAINT_MAX places in a car's list */
#define PAINT_ENTRY(K) \
    static int paint_offered_##K(int model) { return paint_offered(model, K); } \
    static int paint_chosen_##K(int model) { return paint_chosen(model, K); } \
    static void paint_apply_##K(int model, int on) { keep(model, PAINT_CATALOGUE + K, on); } \
    static void paint_draw_##K(const CarVariantCanvas *c, int model, int on) { paint_draw(c, model, K, on); }
PAINT_ENTRY(0)
PAINT_ENTRY(1)
PAINT_ENTRY(2)
PAINT_ENTRY(3)

#define PAINT_ROW(K) { "paint_" #K, CAR_VARIANT_COLOUR, paint_offered_##K, paint_chosen_##K, paint_apply_##K, paint_draw_##K }

const CarVariant showcase_variants[] = {
    { "ps1_livery", CAR_VARIANT_COLOUR, ps1_offered, ps1_chosen, ps1_apply, ps1_draw },
    PAINT_ROW(0), PAINT_ROW(1), PAINT_ROW(2), PAINT_ROW(3),
};
const int showcase_variant_count = (int)(sizeof showcase_variants / sizeof showcase_variants[0]);

void showcase_variants_reset(void) { ps1.model = -1; }
