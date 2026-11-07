/* port: the catalogue paints of the eight drivable cars (the mode's file car_paint, the car showcase's Color box). Each car has
 * its own PC colour (the choice 0, the exe's) and up to CAR_PAINT_MAX more, colours its real car came in around the game's
 * years (re/notes/car-paint-catalog.md; hex values are estimates from the colours' names). A paint is applied by recolouring
 * the body's entries of the car's palette: its hue and saturation come from the paint, its lightness from the paint scaled by
 * each entry's shading against the body's main tone, so the textures keep their light and shade. Nothing is EA data: the
 * table is the port's own. */
#ifndef TNFS_CAR_PAINT_H
#define TNFS_CAR_PAINT_H

#include <stdint.h>

enum { CAR_PAINT_MODELS = 8, CAR_PAINT_MAX = 4, CAR_PAINT_ENTRIES = 256 };   /* the race models 0..7 (race_models.h's car_model_names) */

typedef struct {
    const char *name;
    uint8_t rgb[3];
} CarPaint;

/* How many catalogue paints the race model has (0 for a model without a table), and the k-th, from 0; NULL past the end. */
int car_paint_count(int model);
const CarPaint *car_paint(int model, int k);

/* The car's black, the catalogue paint a player gets by choosing black (the darkest one whose channels are all 32 or under), by the race model's stem in
 * any case ("ANSX", "p911"); NULL for a car without one, an unknown stem or NULL. */
const CarPaint *car_paint_black_of(const char *stem);

/* The model's own colour as the PC has it (its main body tone), for the box's swatch when no PS1 files are read. */
const uint8_t *car_paint_default(int model);

/* The recolour of a body's colours towards a paint, made once from the body's colours (`rgb`, 8 bits a channel, `n` of them,
 * `weight[i]` the pixels of colour i) and then applied colour by colour: the hue and saturation of the paint, the lightness of
 * the paint scaled by each colour's lightness against the body's main tone (the weighted median, which maps to the paint itself),
 * softened, and kept within what the paint's brightness leaves room for, so white keeps its shadows and black its highlights. */
typedef struct {
    float main, room, lift;
    uint8_t rgb[3];
    int ranked;                          /* a near-black body: by the lightness's rank in the body's ramp, not its ratio to the main tone */
    float ramp_main;                     /* the main tone's rank: the gain is 1 there */
    int ramp;                            /* the ramp's entries, darkest first: lightness `ramp_y`, rank (0..1) `ramp_t` */
    float ramp_y[CAR_PAINT_ENTRIES], ramp_t[CAR_PAINT_ENTRIES];
} CarPaintPlan;
void car_paint_plan(CarPaintPlan *plan, const uint8_t (*rgb)[3], const uint32_t *weight, int n, const uint8_t target[3]);
void car_paint_shade(const CarPaintPlan *plan, const uint8_t in[3], uint8_t out[3]);

/* Recolours `n` body entries (`entries`: palette indices) of a CFM palette in place towards the paint `rgb`. `type` is the
 * palette's shape type (0x24: 8 bits a channel, 0x22: 6); another type, a NULL palette or no entries leave it alone.
 * `weight[i]` is how many pixels of the textures use entry i: the body's main tone is the weighted median of the entries'
 * lightness, which maps to the paint itself; the rest are scaled by their lightness against it (softened, and kept within
 * what the paint's brightness leaves room for, so white keeps shadows and black keeps highlights). */
void car_paint_recolour(uint8_t *palette, int type, const uint8_t *entries, int n, const uint32_t weight[256],
                        const uint8_t rgb[3]);

/* port: the car (a race model, car_model_names' order) a slot's race model value is (race_models.c: HUMAN_SET 200 + car for the humans, OTHER_SET 100 + car for the opponents), -1 for traffic and the police */
int car_paint_race_car(int32_t race_model);

/* port (replay colours): a paint a replay recorded for a car (PAINT_OWN, PAINT_PS1, PAINT_CATALOGUE + k), as this machine can show it: the same one, or PAINT_OWN when the PS1 livery's files are not
 * here (`ps1` 0) or the catalogue has no such paint for the car now. -1 for `saved` not a paint (nothing recorded): the caller's own pick stands. */
int car_paint_replayed(int saved, int car, int ps1);

/* The paint a rival takes (the same ids as the player's: 0 the car's own, 1 the PS1 livery when `ps1` says its files are there,
 * 2 + k the k-th of `catalogue` paints): a hash of the race's seed and the rival's slot, so the same race replays with the same
 * colours and the game's own random stream is not drawn from. A rival in the player's car (`car` == `player_car`) never takes the
 * player's paint (`player_paint`) when another is to be had. */
int car_paint_pick(uint32_t seed, int slot, int car, int player_car, int player_paint, int catalogue, int ps1);

/* The paint seed of a race: the race's own seed, except for the 2nd and 3rd segment of a stage set (AL, CL, CY: "CY2" after "CY1"
 * or "CY2"), which keep the one the set's earlier segment drew, so the rivals' colours hold from stage to stage. `keep` is the
 * caller's, one per game; another track (or no earlier segment of the set) takes `race_seed` and starts the set anew. */
typedef struct CarPaintSeedKeep {
    char family[3];
    int have;
    uint32_t seed;
} CarPaintSeedKeep;
uint32_t car_paint_seed(CarPaintSeedKeep *keep, const char *track, uint32_t race_seed);

#endif
