/* port (dep59 3b): the AI drives an added car at the pace and coast of its own tuning (car_registry_ai, from its PDN or its ai_level),
 * never at an original's; the originals keep their tables (MODEL_PACE 004c5ce0, COAST_TICKS 004c5d00), and a number that is no car has -1. */
#include "formats/car_ai.h"
#include "formats/pdn.h"
#include "game/race/ai_manager.h"
#include "game/race/car_registry.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static const int32_t PACE[8] = { 60293, 65536, 60948, 60948, 65536, 62259, 58982, 58982 };
static const int32_t COAST[8] = { 3, 3, 4, 5, 4, 5, 4, 3 };

static int add(const char *id, const char *stem, CarAiTuning ai)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.sound_model = 5, p.dash_model = -1, p.ai = ai, p.fingerprint = 1;
    char why[160] = "";
    return car_registry_add(&p, why, sizeof why);
}

static void test_originals(void)
{
    car_registry_clear();
    for (int car = 0; car < CAR_ORIGINAL_MODELS; car++) {
        CHECK_EQ_INT(ai_model_pace(car), PACE[car]);
        CHECK_EQ_INT(ai_coast_ticks(car), COAST[car]);
        CHECK(car_registry_ai(car) == NULL);
    }
    for (int car = CAR_ORIGINAL_MODELS; car < 12; car++) CHECK_EQ_INT(ai_model_pace(car), -1);   /* traffic and police: no pace */
    CHECK_EQ_INT(ai_model_pace(16), -1);   /* no car registered there */
    CHECK_EQ_INT(ai_coast_ticks(16), -1);
    CHECK_EQ_INT(ai_model_pace(-1), -1);
}

static void test_added(void)
{
    car_registry_clear();
    const int car = add("own", "OWN1", (CarAiTuning){ 61000, 5 });
    CHECK_EQ_INT(car, 16);
    CHECK(car_registry_ai(car) != NULL);
    CHECK_EQ_INT(ai_model_pace(car), 61000);
    CHECK_EQ_INT(ai_coast_ticks(car), 5);
    CHECK_EQ_INT(car_registry_ai_model(car), -1);   /* no original's AI */
    /* Review Focus 4: PDNs of top speed 0 and of the most a word holds: the pace stays in [CAR_AI_PACE_MIN, CAR_AI_PACE_MAX] */
    static const int32_t TOPS[3] = { 0, 0x7fffffff, (int32_t)0x80000000 };
    for (int i = 0; i < 3; i++) {
        uint8_t pdn[PDN_SIZE] = { 0 };
        for (int b = 0; b < 4; b++) pdn[PDN_FACTORS + 20 + b] = (uint8_t)((uint32_t)TOPS[i] >> (8 * b));
        CarAiTuning t;
        char why[96];
        CHECK(car_ai_tuning(pdn, NULL, &t, why, sizeof why));
        char id[8], stem[8];
        snprintf(id, sizeof id, "x%d", i), snprintf(stem, sizeof stem, "X%d", i);
        const int n = add(id, stem, t);
        CHECK(n >= CAR_ADDED_FIRST);
        CHECK(ai_model_pace(n) >= CAR_AI_PACE_MIN && ai_model_pace(n) <= CAR_AI_PACE_MAX);
        CHECK_EQ_INT(ai_coast_ticks(n), CAR_AI_COAST_DEFAULT);
    }
    car_registry_clear();
}

int main(void)
{
    test_originals();
    test_added();
    return TEST_REPORT();
}
