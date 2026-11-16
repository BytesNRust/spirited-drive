/* port (dep59 3b): an added car's AI pace and coast (formats/car_ai.h): the formula from its sixth gear limit, clamped to
 * [CAR_AI_PACE_MIN, CAR_AI_PACE_MAX], and the four named levels. With the CD's PDN files (car_ai_cd): the formula lands within 2300 of
 * the pace the game gives each of the eight cars (MODEL_PACE, ai_manager.c), and the table is printed. */
#include "formats/car_ai.h"
#include "formats/pdn.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TOP_GEAR_LIMIT = PDN_FACTORS + 4 * 5, CD_TOLERANCE = 2300 };

/* the eight cars in model order, with the game's pace for each (004c5ce0, MODEL_PACE in ai_manager.c) */
static const struct { const char *name; int32_t pace; } CARS[] = {
    { "TSUPRA", 60293 }, { "LDIABL", 65536 }, { "P911", 60948 }, { "CZR1", 60948 },
    { "F512TR", 65536 }, { "DVIPER", 62259 }, { "ANSX", 58982 }, { "MRX7", 58982 },
};

static void put_i32(uint8_t *p, int32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)((uint32_t)v >> (8 * i));
}

static CarAiTuning tuning_of_top(int32_t top)
{
    uint8_t pdn[PDN_SIZE] = { 0 };
    put_i32(pdn + TOP_GEAR_LIMIT, top);
    CarAiTuning t = { -1, -1 };
    char why[96] = "";
    CHECK(car_ai_tuning(pdn, NULL, &t, why, sizeof why));
    return t;
}

static void test_clamp(void)
{
    const CarAiTuning zero = tuning_of_top(0), huge = tuning_of_top(0x7fffffff), negative = tuning_of_top(INT32_MIN);
    CHECK_EQ_INT(zero.pace, CAR_AI_PACE_MIN);
    CHECK_EQ_INT(huge.pace, CAR_AI_PACE_MAX);
    CHECK_EQ_INT(negative.pace, CAR_AI_PACE_MIN);
    CHECK_EQ_INT(zero.coast, CAR_AI_COAST_DEFAULT);
    CHECK_EQ_INT(huge.coast, CAR_AI_COAST_DEFAULT);
    CHECK_EQ_INT(CAR_AI_PACE_MIN, 0xe000);
    CHECK_EQ_INT(CAR_AI_PACE_MAX, 0x10000);
    const CarAiTuning mid = tuning_of_top(80 << 16);   /* 39478 + (80 * 289) = 62598: inside the range, unclamped */
    CHECK_EQ_INT(mid.pace, 39478 + 80 * 289);
}

static void check_level(const char *level, int32_t pace, int32_t coast)
{
    uint8_t pdn[PDN_SIZE] = { 0 };
    CarAiTuning t = { -1, -1 };
    char why[96] = "";
    CHECK(car_ai_tuning(pdn, level, &t, why, sizeof why));
    CHECK_EQ_INT(t.pace, pace);
    CHECK_EQ_INT(t.coast, coast);
}

static void test_levels(void)
{
    check_level("slow", 58982, 4);
    check_level("medium", 60948, 4);
    check_level("fast", 62259, 5);
    check_level("top", 65536, 4);
    check_level("", CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT);   /* no level: the formula (top speed 0 here) */

    uint8_t pdn[PDN_SIZE] = { 0 };
    CarAiTuning t = { -1, -1 };
    char why[96] = "";
    CHECK(!car_ai_tuning(pdn, "turbo", &t, why, sizeof why));
    CHECK(strstr(why, "turbo") != NULL);
    char tiny[4] = "xyz";
    CHECK(!car_ai_tuning(pdn, "turbo", &t, tiny, sizeof tiny));   /* a short buffer is cut, still terminated */
    CHECK(memchr(tiny, '\0', sizeof tiny) != NULL);

    CHECK(car_ai_level_ok(NULL));
    CHECK(car_ai_level_ok(""));
    CHECK(car_ai_level_ok("slow") && car_ai_level_ok("medium") && car_ai_level_ok("fast") && car_ai_level_ok("top"));
    CHECK(!car_ai_level_ok("Fast"));
    CHECK(!car_ai_level_ok("turbo"));
    CHECK(!car_ai_level_ok("top "));
}

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = n > 0 ? malloc((size_t)n) : NULL;
    const int got = buf != NULL && fread(buf, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!got) { free(buf); return NULL; }
    *size = (size_t)n;
    return buf;
}

/* argv: the PDN files of the CD; the eight cars among them are each checked once */
static void test_cd(int argc, char **argv)
{
    if (argc < 2) return;
    int seen = 0;
    printf("car      top (16.16)  pace   game   diff\n");
    for (size_t c = 0; c < sizeof CARS / sizeof CARS[0]; ++c) {
        char want[32];
        snprintf(want, sizeof want, "/%s.PDN", CARS[c].name);
        for (int a = 1; a < argc; ++a) {
            const size_t len = strlen(argv[a]), wl = strlen(want);
            if (len < wl || strcmp(argv[a] + len - wl, want) != 0) continue;
            size_t size = 0;
            uint8_t *file = read_file(argv[a], &size), pdn[PDN_SIZE];
            CHECK(file != NULL && pdn_read(pdn, file, size));
            free(file);
            CarAiTuning t = { 0, 0 };
            char why[96] = "";
            CHECK(car_ai_tuning(pdn, NULL, &t, why, sizeof why));
            const int diff = (int)(t.pace - CARS[c].pace);
            printf("%-8s %11d  %5d  %5d  %5d\n", CARS[c].name, (int)pdn_i32(pdn, TOP_GEAR_LIMIT), (int)t.pace, (int)CARS[c].pace, diff);
            if (c == 0) printf("         (mass, PDN +0x14, 16.16: %d; printed to check, not used)\n", (int)pdn_i32(pdn, PDN_MASS));
            CHECK(abs(diff) < CD_TOLERANCE);
            CHECK(t.pace >= CAR_AI_PACE_MIN && t.pace <= CAR_AI_PACE_MAX);
            seen++;
            break;
        }
    }
    CHECK_EQ_INT(seen, (int)(sizeof CARS / sizeof CARS[0]));
}

int main(int argc, char **argv)
{
    test_clamp();
    test_levels();
    test_cd(argc, argv);
    return TEST_REPORT();
}
