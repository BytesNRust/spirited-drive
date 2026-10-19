/* A single race headless: the human (full throttle, wheel straight) and the
 * seven other models as computer racers, each with its PDN. Every computer
 * racer must get going and travel along the road (checks the computer cars,
 * the manager and the contacts hold together in the race loop; the parts
 * are checked one by one against the original by tools/oracle_car.py).
 *   test_race SIMDATA_DIR [TRACK] */
#include "formats/tri.h"
#include "game/race/car_body.h"
#include "game/race/car_load.h"
#include "game/race/race_sim.h"
#include "game/race/skid_marks.h"
#include "game/race/view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    STEER_CENTRE = 0x20,
    THROTTLE = STEER_CENTRE,        /* pedal field 0: full throttle */
    GEAR_UP_KEY = 0x1000,           /* automatic: N to D */
    DRIVE_AT = 320,                 /* race ticks: the gear keys work from 300 */
    SECONDS = 60,
    SINGLE_RACE = 2,
    MIN_NODES = 40
};

static RaceSim sim;
static CarPdn pdns[RACE_PLAYERS];

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;
    uint8_t *data = NULL;
    long length = fseek(fp, 0, SEEK_END) == 0 ? ftell(fp) : -1;
    if (length > 0 && fseek(fp, 0, SEEK_SET) == 0 && (data = malloc((size_t)length)) != NULL &&
        fread(data, 1, (size_t)length, fp) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(fp);
    *size = data != NULL ? (size_t)length : 0;
    return data;
}

static int load_pdn(CarPdn *pdn, const char *simdata, int model)
{
    char path[512];
    snprintf(path, sizeof path, "%s/CARFAMS/%s.PDN", simdata, car_model_names[model]);
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    const int ok = file != NULL && car_pdn_read(pdn, file, size);
    free(file);
    if (!ok) fprintf(stderr, "test_race: cannot read %s\n", path);
    return ok;
}

/* The race's end (00440ef0): not before the human has a finish and has
 * stopped; then 1, or 0x10 when out before the start; and the results
 * (0045af80) leave every racer in the order once. */
static int check_end(RaceSim *s)
{
    Car *human = s->race.cars[0];
    static RaceSim before;  /* the race.cars pointers stay on `s`'s slots */
    before = *s;
    s->race.players[0].finish = s->race.tick - 300;
    human->speed = 0x8001;
    race_sim_tick(s, THROTTLE);
    int ok = !race_sim_over(s);
    s->race.players[0].finish = -999;
    s->race.cars[0]->speed = 0;
    race_sim_tick(s, THROTTLE);
    ok &= s->race.end_request == 0x10;
    *s = before;
    s->race.players[0].finish = s->race.tick - 300;
    s->race.cars[0]->speed = 0x8000;
    race_sim_tick(s, THROTTLE);
    ok &= race_sim_over(s) && s->race.end_request == 1 && s->race.end_state == 1;
    race_sim_end(s);
    int seen = 0;
    for (int i = 0; i < RACE_PLAYERS; i++)
        if (s->race.results_order[i] >= 0 && s->race.results_order[i] < RACE_PLAYERS)
            seen |= 1 << s->race.results_order[i];
    ok &= seen == (1 << RACE_PLAYERS) - 1;
    printf("race end: %s, order", ok ? "ok" : "FAIL");
    for (int i = 0; i < RACE_PLAYERS; i++) printf(" %d", s->race.results_order[i]);
    printf("\n");
    return ok;
}


/* The next race on the same sim, a later segment of a championship (type 2,
 * variant 1): the original keeps the race's globals between races, so the
 * grid is the last results' order (0042748c reads 004fa030), the opponents
 * keep their drivers (004fba70) and the player records stay for 0045aab4. */
static int check_carry(RaceSim *s, const Tri *tri, const CarSpecs *specs, RaceField *field)
{
    int16_t order[9];
    int32_t letters[RACE_PLAYERS];
    memcpy(order, s->race.results_order_short, sizeof order);
    memcpy(letters, s->race.driver_letters, sizeof letters);
    const int32_t ghost = s->race.players[0].lap_marks[(0x4c - 4) / 4] = 0x1234;  /* +0x4c, a split kept */
    field->race_type = 2;
    race_sim_start_field(s, tri, "CL2", specs, field, 1, 0);
    int ok = s->race.track_variant == 1;
    for (int k = 0; k < RACE_PLAYERS; k++) ok &= s->race.grid[order[k]] == k;
    for (int i = 1; i < RACE_PLAYERS; i++) ok &= s->race.driver_letters[i] == letters[i];
    ok &= s->race.players[0].lap_marks[(0x4c - 4) / 4] == ghost;
    printf("next segment: grid");
    for (int i = 0; i < RACE_PLAYERS; i++) printf(" %d", s->race.grid[i]);
    printf(", letters");
    for (int i = 0; i < RACE_PLAYERS; i++) printf(" %d", s->race.driver_letters[i]);
    printf("%s\n", ok ? "" : "  FAIL");
    return ok;
}

/* Control Central's No Mercy (0052b306 bit 2, 0042702c's 004fb7d8): the
 * race's flags, and 004fbac0 0 (else 1, not 0042702c's first 4) */
static int check_flags(RaceSim *s, const Tri *tri, const CarSpecs *specs, RaceField *field)
{
    int ok = 1;
    for (uint32_t flags = 0; flags <= 4; flags += 4) {
        field->track_flags = flags;
        race_sim_start_field(s, tri, "TR1", specs, field, 1, 0);
        ok &= s->race.track_flags == flags && s->race.catch_up == (flags == 0);
    }
    RaceField counts = { 0 };
    counts.race_type = 1;
    race_field_counts(&counts, 0, 4, 0);
    ok &= counts.cops == 0 && counts.traffic == 0;
    printf("no mercy: catch up %d, cops %d, traffic %d%s\n", s->race.catch_up, counts.cops, counts.traffic, ok ? "" : "  FAIL");
    return ok;
}

/* A link game dropped (0052b314) stands: 00440ef0 returns before moving any car. */
static int check_dropped(RaceSim *s, const Tri *tri, const CarSpecs *specs, RaceField *field)
{
    enum { TICKS = 200 };
    field->track_flags = 0;
    race_sim_start_field(s, tri, "TR1", specs, field, 1, 0);
    for (int i = 0; i < TICKS; i++) race_sim_tick(s, THROTTLE);
    s->race.playback = 1, s->race.dropped = 1;
    int32_t before[RACE_CARS];
    for (int i = 0; i < s->race.car_count; i++) before[i] = s->race.cars[i]->progress;
    for (int i = 0; i < TICKS; i++) race_sim_tick(s, THROTTLE);
    int moved = 0;
    for (int i = 0; i < s->race.car_count; i++) moved += s->race.cars[i]->progress != before[i];
    printf("dropped link game: %d cars moved%s\n", moved, moved == 0 ? "" : "  FAIL");
    return moved == 0;
}

/* The race's camera op (00404530 on 004f801c): the replay's cuts and a police car placed before the start move
 * the view onto the car they name */
static int check_camera(RaceSim *s, const Tri *tri, const CarSpecs *specs, RaceField *field)
{
    enum { SLOT = 1, KIND = 5 };
    field->track_flags = 0;
    race_sim_start_field(s, tri, "TR1", specs, field, 1, 0);
    s->race.sounds.camera(s->race.sounds.user, SLOT, KIND);
    const int ok = s->race.chase.base == s->race.cars[SLOT];
    printf("camera op follows car %d: %s\n", SLOT, ok ? "yes" : "no  FAIL");
    return ok;
}

/* AL3 keeps the third surface's off_road from AL2 (004fa9b0 a global race_surfaces_init leaves there): its shoulder
 * is snow, dirt puffs and the off-road sounds */
static int check_surfaces(RaceSim *s, const char *simdata, const CarSpecs *specs, RaceField *field)
{
    static const char *const STEMS[2] = { "AL2", "AL3" };
    uint8_t *files[2] = { NULL, NULL };
    Tri tris[2];
    int ok = 1;
    for (int k = 0; k < 2 && ok; k++) {
        char path[512];
        size_t size = 0;
        snprintf(path, sizeof path, "%s/MISC/%s.TRI", simdata, STEMS[k]);
        files[k] = read_file(path, &size);
        ok = files[k] != NULL && tri_open(&tris[k], files[k], size) == TRI_OK;
    }
    if (ok) {
        field->track_flags = 0;
        race_sim_start_field(s, &tris[0], STEMS[0], specs, field, 1, 0);
        race_sim_start_field(s, &tris[1], STEMS[1], specs, field, 1, 0);
        ok = s->race.surfaces[2].off_road == 1;
    }
    printf("AL3 shoulder off road: %d%s\n", s->race.surfaces[2].off_road, ok ? "" : "  FAIL");
    free(files[0]), free(files[1]);
    return ok;
}

/* The skid ring wrapping to 0 has 004596a8 draw mark -1 (the original reads
 * before its table); the port reads its spare row. Under ASan, an overflow. */
typedef struct {
    SkidView view;
    SkidMarks *marks;
    int32_t drawn;
} SkidDraw;

static void skid_draw_op(void *ctx, int32_t index)
{
    SkidDraw *d = ctx;
    static const SkidRing ring = { 0, 0xfff, 0x100 };
    static const int32_t s[2] = { 0, 0 }, t[2] = { 0, 0 };
    d->drawn = index;
    skid_draw(&d->view, &ring, d->marks, index, 0, 0, 0, s, t);
}

static int check_skid_wrap(void)
{
    static uint8_t pixels[64 * 48];
    static const uint8_t remap[256] = { 0 };
    static View view;
    view_init(&view, 0, 0, 64, 48, 96, 96);
    const int32_t cam[3] = { 0, 0, 0 }, m[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
    const ModelView mv = { &view, cam, m, 0 };
    const QuadTarget target = { pixels, 64, 0, 0, 63, 47 };
    static SkidMarks marks;
    skid_reset(&marks);
    marks.index = SKID_RECORDS - 1;
    SkidDraw d = { { &target, remap, &mv, 1, 0 }, &marks, 0 };
    static Car car;
    car.skid_rear = 0x400000, car.tyre_flags_rear = 1, car.node = 50;
    const SkidInputs in = { 0, 0, 0 };
    const SkidOps ops = { &d, skid_draw_op };
    const int32_t forward[3] = { 0, 0, 0x100 }, side[3] = { 0x80, 0, 0 }, point[3] = { 0, 0, 0x100000 };
    skid_record(&ops, &marks, &car, &in, forward, side, point);
    /* the phantom placed in the spare row (its first corner's screen x, word 27), mark 0 untouched */
    const int ok = marks.index == 0 && d.drawn == -1 && marks.marks[SKID_RECORDS][27] != 0 && marks.marks[0][27] == 0;
    printf("skid ring wrap: index %d, drew mark %d%s\n", (int)marks.index, (int)d.drawn, ok ? "" : "  FAIL");
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_race SIMDATA_DIR [TRACK]\n");
        return 2;
    }
    if (!check_skid_wrap()) return 1;
    const char *track = argc > 2 ? argv[2] : "TR1";
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/%s.TRI", argv[1], track);
    size_t size = 0;
    uint8_t *tri_file = read_file(path, &size);
    Tri tri;
    if (tri_file == NULL || tri_open(&tri, tri_file, size) != TRI_OK) {
        fprintf(stderr, "test_race: cannot open %s\n", path);
        free(tri_file);
        return 1;
    }
    snprintf(path, sizeof path, "%s/CARSPECS/%s.PBS", argv[1], car_model_names[0]);
    uint8_t *file = read_file(path, &size);
    CarSpecs specs;
    const int read = file != NULL && car_specs_read(&specs, file, size);
    free(file);
    if (!read) {
        printf("skip (no %s)\n", path);
        free(tri_file);
        return 0;
    }
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = RACE_PLAYERS;
    field.race_type = SINGLE_RACE;
    for (int i = 0; i < RACE_PLAYERS; i++) {
        snprintf(field.entries[i].name, sizeof field.entries[i].name, "Car%d", i);
        field.entries[i].car = i;
        field.entries[i].automatic = 1;
        if (!load_pdn(&pdns[i], argv[1], i)) {
            free(tri_file);
            return 1;
        }
        field.pdns[i] = &pdns[i];
    }
    race_sim_start_field(&sim, &tri, track, &specs, &field, 2, 0);
    int32_t start[RACE_PLAYERS];
    for (int i = 0; i < RACE_PLAYERS; i++) start[i] = sim.race.cars[i]->progress;
    for (int t = 0; t < SECONDS * RACE_SIM_TICKS_PER_SECOND; t++) {
        const int tap = sim.race.tick >= DRIVE_AT && sim.race.tick < DRIVE_AT + 8;
        race_sim_tick(&sim, THROTTLE | (tap ? GEAR_UP_KEY : 0));
    }
    int ok = 1;
    for (int i = 0; i < RACE_PLAYERS; i++) {
        const Car *car = sim.race.cars[i];
        const int moved = car->progress - start[i];
        const int good = i == 0 || moved > MIN_NODES;
        ok &= good;
        printf("%d %-7s physics %d, node %4d, %4d nodes, speed %6.1f m/s%s\n", i, car_model_names[i], car->physics,
               car->node, moved, (i == 0 ? car->speed : car->ai_speed) / 65536.0, good ? "" : "  FAIL");
    }
    ok &= check_end(&sim);
    ok &= check_carry(&sim, &tri, &specs, &field);
    ok &= check_flags(&sim, &tri, &specs, &field);
    ok &= check_dropped(&sim, &tri, &specs, &field);
    ok &= check_surfaces(&sim, argv[1], &specs, &field);
    ok &= check_camera(&sim, &tri, &specs, &field);
    free(tri_file);
    return ok ? 0 : 1;
}
