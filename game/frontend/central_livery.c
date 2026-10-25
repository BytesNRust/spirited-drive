/* port: the car select's Color line (central_livery.h). */
#include "game/frontend/central_livery.h"

#include "game/frontend/ea_lib.h"
#include "game/race/car_livery.h"
#include "game/race/car_load.h"
#include "game/race/race_models.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"
#include "game/sys/text.h"
#include "game/viewer/drive_livery.h"

enum {
    LINE_X = 20, LINE_Y = 299,      /* under the player's photo's "Player" label, above the race type box */
    SWATCH_W = 12, SWATCH_H = 10, SWATCH_GAP = 4,
    PAD_X = 1 << 2,                 /* VirtualPad bit 2 (the camera's X): not a menu button */
    PS1_CARS = 11                   /* cars 0..10 have PS1 files; 11 is the Warrior */
};
static const char LEAD[] = "Color: ", NORMAL[] = "Normal", PS1[] = "PS1", SEPARATOR[] = " / ";

/* BGR555 to the screen's RGB, the 5-bit channels widened to 8 */
static int32_t rgb_of(uint16_t bgr)
{
    const int32_t r = bgr & 31, g = (bgr >> 5) & 31, b = (bgr >> 10) & 31;
    return ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
}

static void swatch(CentralRun *r, int32_t x, int32_t y)
{
    const ControlCentralOps *o = r->ops;
    const int32_t colour = ea_colour(rgb_of(r->livery_colour));
    for (int32_t i = 0; i < SWATCH_H / 2; i++)
        o->frame_rect(o->ctx, x + i, y + i, x + SWATCH_W - 1 - i, y + SWATCH_H - 1 - i, colour);
}

static int32_t text_w(CentralRun *r, const char *text) { return r->ops->text_width(r->ops->ctx, text); }

/* The choice is kept by the race's car model (the race takes the player's car by it, race_head_to_head_model), so the
 * car select's index is turned into the model first. */
static int model_of(int car) { return race_head_to_head_model((int16_t)car); }

/* the car's PS1 livery and the player's choice for it, read when the car changes */
static void read_car(CentralRun *r, int car)
{
    const PortSettings settings = port_settings_load();
    const int model = model_of(car);
    r->livery_car = car + 1;
    r->livery_ps1 = 0;
    r->livery_pick = 0;
    r->redraw = 1;
    if (car < 0 || car >= PS1_CARS || model < 0 || model >= CAR_MODELS || settings.psx_dir[0] == '\0') return;
    r->livery_ps1 = drive_livery_swatch(settings.psx_dir, car_model_names[model], &r->livery_colour);
    r->livery_pick = r->livery_ps1 != 0 && port_extras_psx_livery_car(model) != 0;
}

void central_livery_init(CentralRun *r)
{
    r->livery_on = port_extras_load().psx_liveries != 0;
}

void central_livery_turn(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (r->livery_on == 0) return;
    const int car = *st->player_car;
    if (r->livery_car != car + 1) read_car(r, car);
    const int held = o->key_down(o->ctx, o->scancode(o->ctx, 'M')) != 0 || (o->pad_buttons(o->ctx) & PAD_X) != 0;
    if (!car_livery_pressed(held, &r->livery_held) || r->livery_ps1 == 0) return;
    r->livery_pick = !r->livery_pick;
    port_extras_set_psx_livery_car(model_of(car), r->livery_pick);
    r->redraw = 1;
}

void central_livery_draw(CentralRun *r)
{
    const ControlCentralOps *o = r->ops;
    if (r->livery_on == 0 || r->livery_car == 0) return;
    const int picked = r->livery_pick != 0;
    int32_t x = LINE_X;
    o->plus_text(o->ctx, TEXT_CREAM, LEAD, x, LINE_Y);
    x += text_w(r, LEAD);
    o->plus_text(o->ctx, picked ? TEXT_CREAM : TEXT_YELLOW, NORMAL, x, LINE_Y);
    x += text_w(r, NORMAL);
    if (r->livery_ps1 == 0) return;
    o->plus_text(o->ctx, TEXT_CREAM, SEPARATOR, x, LINE_Y);
    x += text_w(r, SEPARATOR);
    swatch(r, x, LINE_Y);
    x += SWATCH_W + SWATCH_GAP;
    o->plus_text(o->ctx, picked ? TEXT_YELLOW : TEXT_CREAM, PS1, x, LINE_Y);
}
