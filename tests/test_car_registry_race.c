/* dep59 step 1: the race takes an added car's files, engine sound and pace from the registry (car_registry.h): car_load_path names its
 * own CFM (the full one for a rival too: an added car brings no reduced model), its sound is its sound_from's bank, the AI's row is its
 * ai_from's. For the twelve of the exe each answer is what it was; a number past them that is not registered is still no car. */
#include "game/race/ai_setup.h"
#include "game/race/car_load.h"
#include "game/race/car_model.h"
#include "game/race/car_registry.h"
#include "game/race/race_models.h"
#include "game/race/race_sound.h"
#include "game/race/race_state.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { OTHER_SET = 100, HUMAN_SET = 200 };

static RaceState race;
static Car car, lead;

static void register_f40(void)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "f40");
    snprintf(p.stem, sizeof p.stem, "F40");
    snprintf(p.short_name, sizeof p.short_name, "F40");
    p.sound_model = 4, p.ai = (CarAiTuning){ CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, p.dash_model = -1;
    char why[160];
    car_registry_clear();
    CHECK_EQ_INT(car_registry_add(&p, why, sizeof why), 16);
}

static int ends_with(const char *s, const char *tail)
{
    const size_t n = strlen(s), t = strlen(tail);
    return n >= t && strcmp(s + n - t, tail) == 0;
}

/* car_load_path for race model `model`: its answer, with `path`, `stem` and the car's model */
static int path_of(int model, char path[CAR_LOAD_PATH], char stem[16])
{
    memset(&race, 0, sizeof race);
    memset(&car, 0, sizeof car);
    car.model = -1;
    race.models[0] = model;
    path[0] = stem[0] = '\0';
    return car_load_path(&car, &race, "dir/", path, stem);
}

static void test_the_files(void)
{
    char path[CAR_LOAD_PATH], stem[16];
    register_f40();
    CHECK_EQ_INT(path_of(HUMAN_SET + 16, path, stem), 1);
    CHECK(strcmp(path, "dir/F40.cfm") == 0);
    CHECK(strcmp(stem, "F40") == 0);
    CHECK_EQ_INT(car.model, 16);
    CHECK_EQ_INT(path_of(OTHER_SET + 16, path, stem), 1);   /* a rival: the same full model, no 'm' */
    CHECK(strcmp(path, "dir/F40.cfm") == 0);
    CHECK_EQ_INT(car.model, 16);
    CHECK_EQ_INT(path_of(HUMAN_SET + 17, path, stem), 0);   /* not registered */
    CHECK_EQ_INT(path_of(OTHER_SET + 12, path, stem), 0);
    CHECK_EQ_INT(path_of(HUMAN_SET + 5, path, stem), 1);    /* the originals as they were */
    CHECK(strcmp(path, "dir/DVIPER.cfm") == 0);
    CHECK_EQ_INT(path_of(OTHER_SET + 5, path, stem), 1);
    CHECK(strcmp(path, "dir/mDVIPER.cfm") == 0);
    CHECK_EQ_INT(path_of(HUMAN_SET + 11, path, stem), 1);
    CHECK(ends_with(path, "Traffc.cfm"));
    car_registry_clear();
    CHECK_EQ_INT(path_of(HUMAN_SET + 16, path, stem), 0);   /* gone with the registry */
}

typedef struct {
    char names[8][32];
    int count;
} Loaded;

static void load_name(void *user, const char *name)
{
    Loaded *l = user;
    if (l->count < 8) snprintf(l->names[l->count++], sizeof l->names[0], "%s", name);
}

static Loaded banks_of(int player_car, int other_car)
{
    Loaded l;
    memset(&l, 0, sizeof l);
    memset(&race, 0, sizeof race);
    race.player = 0;
    race.entries[race_slot(0)].sound_on = 1;
    race.entries[race_slot(0)].car = player_car;
    race.entries[1].car = other_car;
    race_sound_load_banks(&race, load_name, &l);
    return l;
}

static void test_the_sound(void)
{
    register_f40();
    Loaded l = banks_of(16, 16);
    CHECK(l.count >= 2 && strcmp(l.names[0], "tr512_sw.bnk") == 0 && strcmp(l.names[1], "O512mb.bnk") == 0);
    l = banks_of(5, 2);   /* as before */
    CHECK(l.count >= 2 && strcmp(l.names[0], "Vipersw.bnk") == 0 && strcmp(l.names[1], "O911mb.bnk") == 0);
    {   /* sound_from=traffic (model 11): the traffic car's banks, for the player and for the rival that has it */
        CarPiece t;
        memset(&t, 0, sizeof t);
        snprintf(t.id, sizeof t.id, "tsound");
        snprintf(t.stem, sizeof t.stem, "TSOUND");
        snprintf(t.short_name, sizeof t.short_name, "TSOUND");
        t.sound_model = 11, t.ai = (CarAiTuning){ CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, t.dash_model = -1;
        char why[160];
        const int car = car_registry_add(&t, why, sizeof why);
        CHECK(car >= 16);
        l = banks_of(car, car);
        CHECK(l.count >= 2 && strcmp(l.names[0], "traffc.bnk") == 0 && strcmp(l.names[1], "traffcO.bnk") == 0);
        l = banks_of(2, car);
        CHECK(l.count >= 2 && strcmp(l.names[0], "911_sw.bnk") == 0 && strcmp(l.names[1], "traffcO.bnk") == 0);
    }
    l = banks_of(17, 17);   /* not registered: no bank, as for any number past the twelve */
    CHECK(l.count >= 1 && strcmp(l.names[0], "Vipersw.bnk") != 0 && strstr(l.names[0], "_sw") == NULL && strstr(l.names[0], "mb.bnk") == NULL);
}

static void test_the_pace(void)
{
    register_f40();
    memset(&race, 0, sizeof race);
    memset(&lead, 0, sizeof lead);
    race.race_type = 3;
    race.cars[0] = &lead;
    lead.model = 1;
    const int32_t diablo = ai_profile_row(&race, 0x55);
    CHECK(diablo != 0x55);
    lead.model = 16;
    CHECK_EQ_INT(ai_profile_row(&race, 0x55), 0x55);     /* 3b: an added car takes the fallback, not an original's row */
    lead.model = 17;
    CHECK_EQ_INT(ai_profile_row(&race, 0x55), 0x55);     /* not registered: the fallback, as before */
}

/* dep59 step 3: 00426fec's pick (004fbad0) may be a registered added car, the same number (race_head_to_head_model); one past the
 * table not registered is no pick (-1), as before. A single race with two humans races it. */
static void test_the_pick(void)
{
    register_f40();
    enum { SINGLE_RACE = 2, RACERS = 4 };
    RaceState *r = &race;
    memset(r, 0, sizeof *r);
    r->race_type = SINGLE_RACE, r->humans = 2, r->racers = RACERS, r->player = 0;
    r->entries[0].car = 4, r->entries[1].car = 3;
    r->unknown_4fbad0 = 16;
    race_models(r);
    for (int i = 2; i < RACERS; i++) CHECK_EQ_INT(r->models[i], 16 + HUMAN_SET);
    memset(r, 0, sizeof *r);
    r->race_type = SINGLE_RACE, r->humans = 2, r->racers = RACERS, r->player = 0;
    r->entries[0].car = 4, r->entries[1].car = 3;
    r->unknown_4fbad0 = 40;   /* not registered */
    race_models(r);
    for (int i = 2; i < RACERS; i++) CHECK_EQ_INT(r->models[i], 0);   /* left unset, as a pick past the table */
    car_registry_clear();
}

int main(void)
{
    test_the_pick();
    test_the_files();
    test_the_sound();
    test_the_pace();
    return TEST_REPORT();
}
