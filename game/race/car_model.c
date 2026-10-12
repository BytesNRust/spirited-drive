#include "game/race/car_model.h"

#include "game/sys/shape_bank.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SIZE_SHIFT = 9,
    TRAFFIC_FILES = 12, RACER_FILES = 12,
    OTHER_SET = 100, HUMAN_SET = 200, POLICE_SET = 400,
    PALETTE_BYTES = 0x300, PALETTE_TYPE_8BIT = 0x24, SHAPE_DATA = 0x10,
    COP_FLAG = 8,
    PART_LIMIT = 8, PART_MISSING = 999,
    TYRE_REFS = 4, TYRE_FRAME_BOUND = 3,
    LIGHTS = 4, LIGHT_POINTS = 2,
    MIRROR_BIT = 0x200
};

/* 004c56a0, 004c5660 (= 004c54d0 + 4 x 100 = 004c5340 + 4 x 200), 004c5690. */
static const char *const TRAFFIC[TRAFFIC_FILES] = { "Wagon", "CRX", "Vandura", "BMW", "SunBird", "JETTA",
                                                    "RODEO", "axxess", "probe", "Jeep", "Lemans", "Pickup" };
static const char *const RACERS[RACER_FILES] = { "TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER",
                                                 "ANSX", "MRX7", "MRX7", "MRX7", "MRX7", "Traffc" };
static const char POLICE[] = "CopMust";
static const uint8_t PAL_NAME[4] = { '!', 'P', 'A', 'L' }, OWN_NAME[4] = { '!', 'x', 'x', 'x' };
/* 004b1188.. and the tables 004c5778, 004c57dc, 004c57ec. */
static const char *const PART_NAMES[CAR_LOAD_PARTS] = { "inside", "outside", "surface", "convex" };
static const char *const TYRES[TYRE_REFS] = { "tire1", "tire2", "tire3", "tire4" };
static const char *const LIGHT_LABELS[LIGHTS] = { "lt_frnt", "rt_frnt", "lt_rear", "rt_rear" };
static const char *const LIGHT_POINT_NAMES[LIGHTS * LIGHT_POINTS] = { "fl0", "fl1", "fr0", "fr1",
                                                                      "rl0", "rl1", "rr0", "rr1" };

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

int car_load_path(Car *car, RaceState *race, const char *dir, char path[CAR_LOAD_PATH], char stem[16])
{
    const int32_t slot = race_slot(car->index);
    const int32_t model = race->models[slot];
    const char *name, *prefix = "";
    if (model >= POLICE_SET) {
        name = POLICE;
        race->models[slot] = model - POLICE_SET;
    } else if (model >= HUMAN_SET) {
        if (model - HUMAN_SET >= RACER_FILES) return 0;  /* port guard */
        name = RACERS[model - HUMAN_SET];
        car->model = model - HUMAN_SET;
    } else if (model >= OTHER_SET) {
        if (model - OTHER_SET >= RACER_FILES) return 0;  /* port guard */
        name = RACERS[model - OTHER_SET], prefix = "m";
        car->model = model - OTHER_SET;
    } else {
        if (model < 0 || model >= TRAFFIC_FILES) return 0;  /* port guard */
        name = TRAFFIC[model];
    }
    snprintf(stem, 16, "%s", name);
    snprintf(path, CAR_LOAD_PATH, "%s%s%s.cfm", dir, prefix, name);
    return 1;
}

void car_load_pdn_path(const char *dir, const char *stem, char path[CAR_LOAD_PATH])
{
    snprintf(path, CAR_LOAD_PATH, "%s%s.PDN", dir, stem);
}

/* A shape found by name in a level's bank, writable in the file. */
static uint8_t *bank_shape(const CarLoad *m, int level, const uint8_t name[4])
{
    if (!m->has_bank[level]) return NULL;
    return shape_bank_find((uint8_t *)(uintptr_t)m->bank[level].data, name);
}

/* A 0x24 palette is made 6-bit (each byte >> 2), then the shape is tinted
 * (004473d8). `colours` NULL: the original writes through a stale +0x526. */
static void palette_ready(RacePalette *pal, uint8_t *shape, uint8_t *colours)
{
    if (shape[0] == PALETTE_TYPE_8BIT && colours != NULL)
        for (int i = 0; i < PALETTE_BYTES; i++) colours[i] = (uint8_t)(colours[i] >> 2);
    race_palette_tint_shape(pal, shape);
}

/* Children 2k (ORIP) and 2k + 1 (SHPI) of the CFM, and the bank's palette. */
static CarLoadResult open_level(CarLoad *m, RacePalette *pal, int level)
{
    Chunk orip, shpi;
    if (chunk_child(&m->file, 2 * (uint32_t)level, &orip) != CHUNK_OK || mesh_open(&m->mesh[level], &orip) != MESH_OK)
        return CAR_LOAD_FILE;
    m->has_bank[level] = chunk_child(&m->file, 2 * (uint32_t)level + 1, &shpi) == CHUNK_OK &&
                         shpi.kind == CHUNK_KIND_BITMAP && fsh_open(&m->bank[level], shpi.data, shpi.size) == FSH_OK;
    uint8_t *shape = bank_shape(m, level, PAL_NAME);
    if (shape == NULL) return CAR_LOAD_PALETTE;
    m->palette[level] = shape + SHAPE_DATA;
    palette_ready(pal, shape, m->palette[level]);
    return CAR_LOAD_OK;
}

/* The '!xxx' colours (the high bank): kept for cars not flagged 8, made
 * 6-bit and tinted for all. */
static CarLoadResult own_colours(const Car *car, CarLoad *m, RacePalette *pal)
{
    uint8_t *shape = bank_shape(m, 0, OWN_NAME);
    if (shape == NULL) return CAR_LOAD_PALETTE;
    if (!(car->flags_174 & COP_FLAG)) m->own = shape + SHAPE_DATA;
    palette_ready(pal, shape, m->own);
    return CAR_LOAD_OK;
}

/* 0048d798 (with 00490cc0) and 00490b9a for a level. */
static CarLoadResult make_object(CarLoad *m, const PaletteMatch *match, int level)
{
    Object3D *obj = &m->object[level];
    if (!object3d_create(obj, &m->mesh[level])) return CAR_LOAD_MEMORY;
    if (match != NULL) object3d_match_colours(obj, match);
    if (m->has_bank[level]) object3d_bind(obj, &m->bank[level]);
    return CAR_LOAD_OK;
}

/* 004fbad8: bit 0x200 of the first +8 (order_count) polygons flipped. */
static void mirror(Object3D *obj)
{
    for (uint32_t i = 0; i < obj->order_count; i++) obj->polygons[i].flags ^= MIRROR_BIT;
}

/* 004911f2 / 0049126b: inside and surface must be below 8; outside and
 * convex become 999 with a count of 0 otherwise (their lists stay as they were).
 * A list found stays in `m`; its ref field in the car is the caller's. */
static CarLoadResult parts(Car *car, CarLoad *m)
{
    int32_t index[CAR_LOAD_PARTS], count[CAR_LOAD_PARTS];
    int counted[CAR_LOAD_PARTS] = { 0 };
    CarLoadResult r = CAR_LOAD_OK;
    for (int k = 0; k < CAR_LOAD_PARTS; k++) index[k] = object3d_section(&m->object[0], PART_NAMES[k]);
    for (int k = 0; k < CAR_LOAD_PARTS && r == CAR_LOAD_OK; k++) {
        const int required = k == 0 || k == 2;
        if ((uint32_t)index[k] >= PART_LIMIT) {
            if (required) {
                r = CAR_LOAD_HANGS;
                break;
            }
            index[k] = PART_MISSING, count[k] = 0, counted[k] = 1;
        } else if (!object3d_section_list(&m->object[0], index[k], &m->parts[k])) {
            r = CAR_LOAD_MEMORY;
        } else {
            count[k] = (int32_t)m->parts[k].count, counted[k] = 1;
        }
    }
    /* the packed record's fields one by one (no pointers into it) */
    car->inside_part = index[0], car->outside_part = index[1], car->surface_part = index[2], car->convex_part = index[3];
    if (counted[0]) car->inside_count = count[0];
    if (counted[1]) car->outside_count = count[1];
    if (counted[2]) car->surface_count = count[2];
    if (counted[3]) car->convex_count = count[3];
    return r;
}

/* Models 200 and up: the tyre refs, the four wheel frames (the port keeps
 * the ref index of each, the original its shape), frame 3 bound to both. */
static CarLoadResult tyres(Car *car, const RaceState *race, CarLoad *m)
{
    Object3D *obj = &m->object[0];
    car->front_tyres = object3d_texture_named(obj, "frntires");
    car->rear_tyres = object3d_texture_named(obj, "rertires");
    for (int k = 0; k < TYRE_REFS; k++) {
        const int32_t ref = object3d_texture_named(obj, TYRES[k]);
        if (ref < 0 && race->player == car->index) return CAR_LOAD_HANGS;
        car->wheel_frames[k] = (uint32_t)ref;  /* -1: the original reads the word before the slots */
    }
    const uint32_t frame = car->wheel_frames[TYRE_FRAME_BOUND];
    const FshEntry *shape = frame < obj->texture_count ? obj->textures[frame].texture : NULL;
    object3d_set_texture(obj, car->front_tyres, shape);
    car->front_frame = TYRE_FRAME_BOUND;
    object3d_set_texture(obj, car->rear_tyres, shape);
    car->rear_frame = TYRE_FRAME_BOUND;
    return CAR_LOAD_OK;
}

/* The followed car's turn signals, exhaust point and lights. */
static CarLoadResult signals(Car *car, CarLoad *m)
{
    const Object3D *obj = &m->object[0];
    car->left_turn = object3d_part_named(obj, "left_turn");
    car->right_turn = object3d_part_named(obj, "right_turn");
    if (car->left_turn < 0 || car->right_turn < 0) return CAR_LOAD_HANGS;
    car->smoke_point = object3d_point_named(obj, "smok");
    if (car->smoke_point < 0) return CAR_LOAD_HANGS;
    for (int i = 0; i < LIGHTS; i++) {
        car->light_labels[i] = object3d_label_named(obj, LIGHT_LABELS[i]);
        if (car->light_labels[i] < 0) return CAR_LOAD_HANGS;
        for (int j = 0; j < LIGHT_POINTS; j++) {
            car->light_points[LIGHT_POINTS * i + j] = object3d_point_named(obj, LIGHT_POINT_NAMES[LIGHT_POINTS * i + j]);
            if (car->light_points[LIGHT_POINTS * i + j] < 0) return CAR_LOAD_HANGS;
        }
    }
    return CAR_LOAD_OK;
}

CarLoadResult car_load_setup(Car *car, RaceState *race, RacePalette *pal, const PaletteMatch *match, CarLoad *m,
                               uint8_t *file, size_t size)
{
    memset(m, 0, sizeof *m);
    if (chunk_open(&m->file, file, size) != CHUNK_OK) return CAR_LOAD_FILE;
    CarLoadResult r = open_level(m, pal, 0);
    if (r == CAR_LOAD_OK) r = own_colours(car, m, pal);
    if (r == CAR_LOAD_OK) r = open_level(m, pal, 1);
    for (int level = 0; level < CAR_LOAD_LEVELS && r == CAR_LOAD_OK; level++) r = make_object(m, match, level);
    if (r != CAR_LOAD_OK) return r;
    if (race->mirrored)
        for (int level = 0; level < CAR_LOAD_LEVELS; level++) mirror(&m->object[level]);
    r = parts(car, m);
    if (r != CAR_LOAD_OK) return r;
    const uint32_t points = m->mesh[0].unknown_14 < m->object[0].vertex_count ? m->mesh[0].unknown_14
                                                                                : m->object[0].vertex_count;  /* port guard */
    car_model_size(car, (const int32_t (*)[3])m->object[0].model, (int32_t)points);
    if (race->models[race_slot(car->index)] >= HUMAN_SET) r = tyres(car, race, m);
    if (r == CAR_LOAD_OK && race->cars[race_slot(race->player)] == car) r = signals(car, m);
    return r;
}

void car_load_free(CarLoad *m)
{
    for (int k = 0; k < CAR_LOAD_PARTS; k++) free(m->parts[k].order);
    for (int level = 0; level < CAR_LOAD_LEVELS; level++) object3d_free(&m->object[level]);
    memset(m, 0, sizeof *m);
}
