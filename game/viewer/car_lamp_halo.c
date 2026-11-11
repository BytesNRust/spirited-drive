#include "game/viewer/car_lamp_halo.h"

#include "game/gfx/raster_record.h"
#include "game/race/car_draw.h"
#include "game/race/lamp_flare.h"
#include "game/race/police_lights.h"
#include "game/sys/palette.h"
#include "game/sys/port_settings.h"
#include "game/viewer/drive_lamps.h"
#include "game/viewer/drive_light.h"
#include "game/race/lamp_emissive.h"

#include <stdint.h>

/* port (lamp_flares): the halo round a lit lamp: the brake light, the reversing light, the headlights, the police car's red and blue. A lit lamp's pixels are an emissive
 * entry of its own (drive_lamps_lit_entry: not tinted, not fogged), found in the rectangle of the car's projected vertices after the car is drawn, so what hides the
 * lamp hides its blobs; the halo is added to the pixels' palette, in the lamp's colour at three strengths over a radius a few times the lamp's own */
typedef struct {
    int entry;
    int entry2;                       /* port: a second entry of the same lamp (a police lamp's centre, 0: none) whose pixels give the halo too */
    int percent[LAMP_FLARE_LEVELS];   /* the lamp's colour added to the pixel: a soft glow stronger toward the lamp */
    int reach;                        /* port: the halo's radius in percent of the usual (0 or 100: usual); the headlights' and the police lights' are powerful lights, bigger */
} FlareLamp;

enum { BIG_REACH_HEADLIGHT = 180, BIG_REACH_POLICE = 260, FLARE_NEAR_Z = 64 };   /* FLARE_NEAR_Z: the camera z (16.16 units) below which a vertex does not give the car's rectangle */

/* a halo of `lamp`'s colour at (cx, cy), `radius` pixels: the pixels it covers go to the palette's entry nearest their colour plus the lamp's (the lamp's own colour stays) */
static void flare_halo(const QuadTarget *t, const uint8_t palette[768], const FlareLamp *lamp, int cx, int cy, int radius)
{
    uint8_t member[256] = { 0 };
    member[lamp->entry] = 1;
    const uint8_t *const usable = palette_usable();
    uint8_t maps[LAMP_FLARE_LEVELS][256];
    for (int l = 0; l < LAMP_FLARE_LEVELS; l++)
        for (int d = 0; d < 256; d++) {
            int rgb[3];
            for (int k = 0; k < 3; k++) {
                rgb[k] = palette[3 * d + k] + palette[3 * lamp->entry + k] * lamp->percent[l] / 100;
                if (rgb[k] > 255) rgb[k] = 255;
            }
            maps[l][d] = member[d] || !usable[d] ? (uint8_t)d : (uint8_t)palette_nearest(palette, usable, rgb[0], rgb[1], rgb[2]);
        }
    const uint8_t *const map[LAMP_FLARE_LEVELS] = { maps[0], maps[1], maps[2] };
    lamp_halo_draw(t->pixels, t->pitch, t->clip_left, t->clip_top, t->clip_right - 1, t->clip_bottom - 1, cx, cy, radius, map);
}

static int flare_lamp(const QuadTarget *t, const uint8_t palette[768], const int rect[4], const FlareLamp *lamp)
{
    uint8_t member[256] = { 0 };
    member[lamp->entry] = 1;
    if (lamp->entry2 != 0) member[lamp->entry2] = 1;
    LampBlob blobs[LAMP_FLARE_BLOBS];
    const int count = lamp_blobs(t->pixels, t->pitch, rect[0], rect[1], rect[2], rect[3], member, blobs, LAMP_FLARE_BLOBS);
    for (int i = 0; i < count; i++)
        flare_halo(t, palette, lamp, blobs[i].cx, blobs[i].cy, lamp_flare_radius_scaled(blobs[i].area, lamp->reach));
    return count;
}

/* port (traffic_halo): 1 when the model's nose (+z) is nearer the camera than its tail: a car coming at the camera, whose headlights it can see */
static int faces_camera(const Object3D *o)
{
    if (o->vertex_count == 0 || o->model == NULL || o->camera == NULL) return 0;
    uint32_t front = 0, tail = 0;
    for (uint32_t v = 1; v < o->vertex_count; v++) {
        if (o->model[v][2] > o->model[front][2]) front = v;
        if (o->model[v][2] < o->model[tail][2]) tail = v;
    }
    return o->camera[front][2] < o->camera[tail][2];
}


/* port (plate_light): 1 when the model's nose (+z) is farther from the camera than its tail: the rear is what the camera sees, so a lit headlight-class texel in view is the rear plate's (the owner paints
 * two or three in it), not a headlight: it shines as a texel and gives no halo. 0 when there is no depth to tell */
static int faces_away(const Object3D *o)
{
    if (o->vertex_count == 0 || o->model == NULL || o->camera == NULL) return 0;
    return !faces_camera(o);
}

/* the screen rectangle of the body's vertices that are in front of the camera, clipped to the target: 0 when nothing of the car is. *behind: some vertex is at or before the near plane
 * (their screen corners are not a place: the mirror, a car just beside the camera) */
static int flare_body_rect(const Object3D *body, const QuadTarget *t, int rect[4], int *behind)
{
    rect[0] = rect[1] = INT32_MAX;
    rect[2] = rect[3] = INT32_MIN;
    *behind = 0;
    for (uint32_t v = 0; v < body->vertex_count; v++) {
        if (body->camera != NULL && body->camera[v][2] < FLARE_NEAR_Z) {
            *behind = 1;
            continue;
        }
        if (body->screen[v][0] < rect[0]) rect[0] = body->screen[v][0];
        if (body->screen[v][0] > rect[2]) rect[2] = body->screen[v][0];
        if (body->screen[v][1] < rect[1]) rect[1] = body->screen[v][1];
        if (body->screen[v][1] > rect[3]) rect[3] = body->screen[v][1];
    }
    if (rect[0] > rect[2] || rect[1] > rect[3]) return 0;   /* nothing of the car is in front of the camera */
    if (rect[0] < t->clip_left) rect[0] = t->clip_left;
    if (rect[1] < t->clip_top) rect[1] = t->clip_top;
    if (rect[2] >= t->clip_right) rect[2] = t->clip_right - 1;
    if (rect[3] >= t->clip_bottom) rect[3] = t->clip_bottom - 1;
    return 1;
}

/* port (traffic_halo): the far model has no lamp texels: the pair of headlights by the car's rectangle, faded by distance */
static void flare_far_traffic_pair(const QuadTarget *t, const uint8_t palette[768], const int rect[4], int head, const Car *car)
{
    const int fade = lamp_far_fade(car->camera_position[2], car_draw_far_z(car_draw_distance_scale()));
    if (fade <= 0) return;
    const int w = rect[2] - rect[0] + 1, h = rect[3] - rect[1] + 1, base = w / 2 < 6 ? 6 : w / 2 > 11 ? 11 : w / 2;
    const int radius = base * fade / 100 < 2 ? 2 : base * fade / 100;
    const FlareLamp lamp = { head, 0, { 30 * fade / 100, 65 * fade / 100, 110 * fade / 100 }, 100 };
    const int cy = rect[1] + h * 2 / 3;
    flare_halo(t, palette, &lamp, rect[0] + w / 4, cy, radius);
    flare_halo(t, palette, &lamp, rect[0] + w * 3 / 4, cy, radius);
}

/* port (police_halo): a strong red and blue pair over the roof by the car's rectangle, as big as its width reads at that distance; far_percent is the size setting's */
static void flare_far_police_pair(const QuadTarget *t, const uint8_t palette[768], const int rect[4], int red, int blue, int far_percent, const RaceState *shadow_race)
{
    const int w = rect[2] - rect[0] + 1, h = rect[3] - rect[1] + 1, phase = police_lights_phase(shadow_race->tick);
    int radius = w * 7 / 10 < 8 ? 8 : w * 7 / 10 > LAMP_FLARE_MAX_RADIUS ? LAMP_FLARE_MAX_RADIUS : w * 7 / 10;
    radius = radius * far_percent / 100 > LAMP_FLARE_BIG_RADIUS ? LAMP_FLARE_BIG_RADIUS : radius * far_percent / 100;
    const int cy = rect[1] + h / 4;
    const FlareLamp left = { phase == 0 ? red : blue, 0, { 70, 120, 180 }, 100 }, right = { phase == 0 ? blue : red, 0, { 70, 120, 180 }, 100 };
    if (left.entry != 0) flare_halo(t, palette, &left, rect[0] + w / 3, cy, radius);
    if (right.entry != 0) flare_halo(t, palette, &right, rect[0] + w * 2 / 3, cy, radius);
}

void car_lamp_halo_draw(const Object3D *body, const Car *car, const RaceState *shadow_race, int police_flash_on, const QuadTarget *t, const uint8_t palette[768])
{
    if (palette == NULL || body == NULL || body->screen == NULL) return;
    /* the police car is slot 2, past the two cars of an open road's finish_cars: with police_glow its lights get their halo (lamp_flares) */
    const int cop = port_police_glow() && police_flash_on && police_lights_on(shadow_race, car);
    const int traffic_ok = port_traffic_halo() && drive_light_night();   /* port (traffic_halo): the traffic's lit lamps get their halo too, at night */
    if (car->index < 0 || car->index >= RACE_CARS || (car->index >= shadow_race->finish_cars && !cop && !traffic_ok)) return;
    const int far_police = car->far != 0 && police_flash_on && police_lights_on(shadow_race, car);   /* the low detail model has no light texels: a far police car flashes by its rectangle */
    const int far_traffic = car->far != 0 && !far_police && traffic_ok;   /* port (traffic_halo): the far model has no lamp texels: the pair of headlights by the car's rectangle, faded by distance */
    if (car->far != 0 && !far_police && !far_traffic) return;
    FlareLamp lamps[5];
    int n = 0;
    const int brake = drive_lamps_lit_entry(car->index, LAMP_BRAKE), reverse = drive_lamps_lit_entry(car->index, LAMP_REVERSE),
              head = drive_lamps_lit_entry(car->index, LAMP_HEADLIGHT);
    const int no_halo = drive_lamps_no_halo(car->index);   /* port (rain_lamps): lit only by the rain by day: the texels, no halo */
    if (!no_halo && brake != 0) lamps[n++] = (FlareLamp){ brake, 0, { 30, 60, 100 }, 100 };
    if (!no_halo && reverse != 0 && drive_lamps_low(car->index)) lamps[n++] = (FlareLamp){ reverse, 0, { 20, 40, 70 }, 100 };   /* white: softer than the brake's red, which reads on dark paint */
    if (!no_halo && head != 0 && !faces_away(body)) lamps[n++] = (FlareLamp){ head, 0, { 30, 65, 110 }, BIG_REACH_HEADLIGHT };
    const int police = police_flash_on && police_lights_on(shadow_race, car);
    const int red = police ? police_lights_own_entry(1) : 0, blue = police ? police_lights_own_entry(0) : 0;
    if (n == 0 && red == 0 && blue == 0) return;
    int rect[4], behind;
    if (!flare_body_rect(body, t, rect, &behind)) return;
    raster_record_sync(t);   /* what the render bands hold for the target is drawn before its pixels are read (one flush, only for a lit lamp) */
    const int glow = port_police_glow(), low = glow && drive_lamps_low(car->index);
    static const int POLICE_REACH[4] = { BIG_REACH_POLICE, 400, 600, 900 }, POLICE_FAR[4] = { 100, 160, 240, 360 };   /* port (police_halo): the roof lights' halo size, near (percent of the lamp) and far (percent of the pair's) */
    const int size = port_police_halo() < 0 ? 0 : port_police_halo() > 3 ? 3 : port_police_halo();
    const FlareLamp copred = { red, red != 0 ? police_lights_own_centre(1) : 0, { police_glow_percent(low, 0), police_glow_percent(low, 1), police_glow_percent(low, 2) }, POLICE_REACH[size] };
    const FlareLamp copblue = { blue, blue != 0 ? police_lights_own_centre(0) : 0, { copred.percent[0], copred.percent[1], copred.percent[2] }, POLICE_REACH[size] };
    if (red != 0 && !far_police) lamps[n++] = glow ? copred : (FlareLamp){ red, 0, { 30, 60, 100 }, 100 };
    if (blue != 0 && !far_police) lamps[n++] = glow ? copblue : (FlareLamp){ blue, 0, { 30, 60, 100 }, 100 };
    int siren_blobs = 0;
    for (int i = 0; i < n; i++) {
        if (far_traffic) break;   /* no texels to find */
        const int found = flare_lamp(t, palette, rect, &lamps[i]);
        if (lamps[i].entry != 0 && (lamps[i].entry == red || lamps[i].entry == blue)) siren_blobs += found;
    }
    if (far_traffic && head != 0 && !no_halo && !behind && faces_camera(body) && rect[2] > rect[0] && rect[3] > rect[1]) flare_far_traffic_pair(t, palette, rect, head, car);
    /* port (police_halo): the siren's lamp texels are a pixel or two a long way off and then nothing is found, while the headlights' still are: a lit police car whose
     * lights gave no blob has the pair halo by its rectangle, as the low detail model does, so the siren shows as far as the headlights */
    if ((far_police || (cop && !far_traffic && siren_blobs == 0 && (red != 0 || blue != 0))) && !behind && rect[2] > rect[0] && rect[3] > rect[1])   /* a strong red and blue pair over the roof, as big as the car's width reads at that distance */
        flare_far_police_pair(t, palette, rect, red, blue, POLICE_FAR[size], shadow_race);
}

