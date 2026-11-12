/* port (chase_steady): the follow camera's chase eye holds the car still in the picture as the car bounces. A car carried
 * along TR2's road pitching and rolling a little back and forth every tick, as a bumpy road shakes it: the exe's chase is
 * rigid with the car (the eye turned by its pitch and roll, the aim with them), so the car keeps its place on the screen.
 * The follow camera aimed its heading at a point ahead from an eye the roll swings sideways, and with chase_height held the
 * eye's height while the aim pitched with the car: the car jumped about the screen at every bump. chase_steady keeps the
 * heading the follow yaw and lets the height reach the eye smoothly.
 *   test_chase_steady SIMDATA_DIR */
#include "formats/tri.h"
#include "game/race/car_body.h"
#include "game/race/car_load.h"
#include "game/race/race_chase.h"
#include "game/race/race_sim.h"
#include "game/race/track_render.h"
#include "game/sys/fixed_math.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TICKS = 240, SETTLE = 30, START_NODE = 300, BOUNCE = 0x20000, SINGLE_RACE = 2, SEED = 0x2468ace, CHASE_KIND = 2 };

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

/* the car on the road a third of a node a tick from START_NODE, heading along it, pitched and rolled by +-BOUNCE in turn */
static void place(Car *car, const RaceState *race, int tick)
{
    const int32_t node = START_NODE + tick / 3, part = tick % 3;
    const TriNode a = race_node(race, node), b = race_node(race, node + 1);
    car->x = a.position.x + (int32_t)(((int64_t)b.position.x - a.position.x) * part / 3);
    car->y = a.position.y + (int32_t)(((int64_t)b.position.y - a.position.y) * part / 3);
    car->z = a.position.z + (int32_t)(((int64_t)b.position.z - a.position.z) * part / 3);
    car->node = node;
    car->lateral = 0;
    car->yaw = (int32_t)((uint32_t)fx_atan2((b.position.x - a.position.x) >> 4, (b.position.z - a.position.z) >> 4) << 8) & 0xffffff;
    car->yaw_rate = 0;
    const int32_t shake = tick % 2 == 0 ? BOUNCE : -BOUNCE;
    car->pitch = car->draw_pitch = shake;   /* signed, as the car keeps them */
    car->roll = car->draw_roll = shake;
}

/* the car's place on the screen (camera x / z, y / z, in thousandths) as the chase camera of this tick sees it: the camera's
 * matrix applied in floating point, finer than the draw's world >> 9, so a jump of a pixel shows */
static void on_screen(const RaceState *race, const Car *car, double out[2])
{
    int32_t m[9];
    track_camera_matrix(race->chase.angles[0], race->chase.angles[1], race->chase.angles[2], 0, 0x10000, m);
    const int32_t at[3] = { car->x, car->y, car->z };
    double rel[3], cam[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) rel[i] = (double)(int32_t)((uint32_t)at[i] - (uint32_t)race->chase.position[i]);
    int32_t probe[3], unit[3];
    for (int j = 0; j < 3; j++) {   /* the matrix's columns as mat3_apply uses them: the image of each axis */
        for (int i = 0; i < 3; i++) unit[i] = i == j ? 0x10000 : 0;
        mat3_apply(unit, m, probe);
        for (int i = 0; i < 3; i++) cam[i] += rel[j] * probe[i] / 65536.0;
    }
    out[0] = cam[2] != 0 ? 1000.0 * cam[0] / cam[2] : 0;
    out[1] = cam[2] != 0 ? 1000.0 * cam[1] / cam[2] : 0;
}

/* the mean of the car's jumps on the screen from one tick to the next beyond the steady drift (the second difference),
 * across and up, with the follow camera, chase_height and chase_steady as given */
static void jitter(int follow, int height, int steady, double out[2])
{
    RaceState *race = &sim.race;
    race->port_follow_camera = follow, race->port_chase_height = height, race->port_chase_steady = steady;
    Car *car = race->cars[race_slot(race->player)];
    place(car, race, 0);
    race_chase_follow(race, &race->chase, car, CHASE_KIND);
    double s[TICKS][2];
    for (int t = 0; t < TICKS; t++) {
        place(car, race, t);
        race_chase_tick(race, &race->chase);
        on_screen(race, car, s[t]);
    }
    out[0] = out[1] = 0;
    for (int t = SETTLE + 1; t < TICKS - 1; t++)
        for (int k = 0; k < 2; k++) out[k] += fabs(s[t - 1][k] - 2 * s[t][k] + s[t + 1][k]) / (TICKS - SETTLE - 2);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_chase_steady SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR2.TRI", argv[1]);
    size_t size = 0;
    uint8_t *tri_file = read_file(path, &size);
    Tri tri;
    if (tri_file == NULL || tri_open(&tri, tri_file, size) != TRI_OK) {
        printf("skip (no %s)\n", path);
        free(tri_file);
        return 0;
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
    field.seed = SEED;
    for (int i = 0; i < RACE_PLAYERS; i++) {
        snprintf(field.entries[i].name, sizeof field.entries[i].name, "Car%d", i);
        field.entries[i].car = i;
        field.entries[i].automatic = 1;
        snprintf(path, sizeof path, "%s/CARFAMS/%s.PDN", argv[1], car_model_names[i]);
        uint8_t *f = read_file(path, &size);
        const int ok = f != NULL && car_pdn_read(&pdns[i], f, size);
        free(f);
        if (!ok) {
            printf("skip (no %s)\n", path);
            free(tri_file);
            return 0;
        }
        field.pdns[i] = &pdns[i];
    }
    race_sim_start_field(&sim, &tri, "TR2", &specs, &field, 2, 0);

    double exe[2], follow[2], height[2], steady[2], steady_height[2];
    jitter(0, 0, 0, exe);
    jitter(1, 0, 0, follow);
    jitter(1, 1, 0, height);
    jitter(1, 0, 1, steady);
    jitter(1, 1, 1, steady_height);
    printf("car's jump on screen (across, up): exe %.2f %.2f, follow %.2f %.2f, with chase_height %.2f %.2f; "
           "chase_steady %.2f %.2f, with chase_height %.2f %.2f\n",
           exe[0], exe[1], follow[0], follow[1], height[0], height[1], steady[0], steady[1], steady_height[0], steady_height[1]);
    /* the exe's chase moves the car a little too (its aim pitches a quarter more than the car); the follow camera's moves it
     * more than twice as much across, and with chase_height more than three times as much up and down: the shake the owner saw */
    CHECK(follow[0] > 2 * exe[0]);
    CHECK(height[1] > 3 * exe[1]);
    for (int k = 0; k < 2; k++) {                         /* chase_steady: as still as the exe's chase */
        CHECK(steady[k] <= 1.25 * exe[k] + 1);
        CHECK(steady_height[k] <= 1.25 * exe[k] + 1);
    }
    free(tri_file);
    return TEST_REPORT();
}
