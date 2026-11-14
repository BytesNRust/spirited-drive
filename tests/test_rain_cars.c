/* the cars' own sounds in the rain (rain_cars.h): the splashes' dice and the few voices the loudest cars get */
#include "tests/test.h"

#include "game/race/rain_cars.h"
#include "game/race/rain_roll.h"
#include "game/race/tick_rate.h"

#include <string.h>

static void test_dice(void)
{
    uint32_t a = 0, b = 0;
    CHECK(rain_dice(&a) != 0 && a != 0);   /* 0 is taken as 1 */
    a = 0;
    int same = 1;
    for (int i = 0; i < 1000; i++) same &= rain_dice(&b) == rain_dice(&a);   /* one seed, one sequence */
    CHECK_EQ_INT(same, 1);
}

static void test_puddle_rate(void)
{
    CHECK_EQ_INT(rain_puddle_millihertz(0), 0);
    CHECK_EQ_INT(rain_puddle_millihertz(RAIN_PUDDLE_FROM), 0);   /* slow: none */
    CHECK_EQ_INT(rain_puddle_millihertz(RAIN_ROLL_FULL), RAIN_PUDDLE_TOP_MILLIHERTZ);
    CHECK_EQ_INT(rain_puddle_millihertz(RAIN_ROLL_FULL * 2), RAIN_PUDDLE_TOP_MILLIHERTZ);
    CHECK_EQ_INT(rain_puddle_millihertz(-RAIN_ROLL_FULL), RAIN_PUDDLE_TOP_MILLIHERTZ);
    int last = 0;
    for (int32_t v = 0; v <= RAIN_ROLL_FULL; v += 0x10000) {
        const int r = rain_puddle_millihertz(v);
        CHECK(r >= last);
        last = r;
    }
    CHECK(RAIN_PUDDLE_TOP_MILLIHERTZ == 1000);   /* about one a second at speed */
}

/* the splashes over a long drive: about the rate, each gain within -6..0 dB, each rate within 5 %, every clip used, never the same twice in a row */
static void test_puddle_roll(void)
{
    uint32_t seed = 12345;
    int hits = 0, last = -1, used[7] = { 0 }, low = 1000, high = 0, slow = 2000, fast = 0;
    const int seconds = 2000;
    for (int t = 0; t < seconds * RACE_TICKS_A_SECOND / 2; t++) {   /* a frame every two ticks */
        RainPuddle p;
        if (!rain_puddle_roll(&seed, RAIN_ROLL_FULL, 2, 7, last, &p)) continue;
        hits++;
        CHECK(p.clip >= 0 && p.clip < 7 && p.clip != last);
        used[p.clip]++, last = p.clip;
        low = p.gain_permille < low ? p.gain_permille : low, high = p.gain_permille > high ? p.gain_permille : high;
        slow = p.pitch_permille < slow ? p.pitch_permille : slow, fast = p.pitch_permille > fast ? p.pitch_permille : fast;
    }
    CHECK(hits > seconds * 9 / 10 && hits < seconds * 11 / 10);
    for (int c = 0; c < 7; c++) CHECK(used[c] > hits / 14);
    CHECK(low >= RAIN_PUDDLE_GAIN_LOW && low < 520 && high <= RAIN_PUDDLE_GAIN_HIGH && high > 980);
    CHECK(slow >= 950 && slow < 955 && fast <= 1050 && fast > 1045);
    RainPuddle p;
    int none = 0;
    for (int t = 0; t < 10000; t++) none += rain_puddle_roll(&seed, RAIN_PUDDLE_FROM, 2, 7, -1, &p);
    CHECK_EQ_INT(none, 0);
    uint32_t s1 = 99, s2 = 99;   /* the draws do not depend on the speed: one car's speed does not change another's dice */
    (void)rain_puddle_roll(&s1, 0, 2, 7, -1, &p), (void)rain_puddle_roll(&s2, RAIN_ROLL_FULL, 2, 7, -1, &p);
    CHECK(s1 == s2);
    CHECK_EQ_INT(rain_puddle_roll(&seed, RAIN_ROLL_FULL, 600, 0, -1, &p), 0);   /* no clips: none */
}

static int voices_on(const RainCarVoices *v)
{
    int n = 0;
    for (int i = 0; i < RAIN_CARS_VOICES; i++) n += v->car[i] >= 0;
    return n;
}

static int voice_of(const RainCarVoices *v, int car)
{
    for (int i = 0; i < RAIN_CARS_VOICES; i++)
        if (v->car[i] == car) return i;
    return -1;
}

static void test_voices(void)
{
    RainCarVoices v;
    rain_car_voices_clear(&v);
    const int all[] = { 3, 0, 5, 1, 7, 2, 4, 6, 8 };
    rain_car_voices_assign(&v, all, 9, 100);   /* the loudest four, each fresh */
    CHECK_EQ_INT(voices_on(&v), RAIN_CARS_VOICES);
    for (int k = 0; k < RAIN_CARS_VOICES; k++) CHECK(voice_of(&v, all[k]) >= 0 && v.fresh[voice_of(&v, all[k])] == 1);
    const int keep_3 = voice_of(&v, 3);
    rain_car_voices_assign(&v, all, 9, 101);   /* the same: kept, not fresh */
    CHECK(voice_of(&v, 3) == keep_3 && v.fresh[keep_3] == 0);
    const int swapped[] = { 3, 0, 5, 7, 1, 2, 4, 6, 8 };   /* 1 drops to fifth: it keeps its voice, 7 waits */
    rain_car_voices_assign(&v, swapped, 9, 102);
    CHECK(voice_of(&v, 1) >= 0 && voice_of(&v, 7) < 0);
    const int gone[] = { 3, 0, 5, 7, 2, 4, 1 };   /* 1 drops to seventh: it loses it, 7 waits for the voice to rest */
    rain_car_voices_assign(&v, gone, 7, 103);
    CHECK(voice_of(&v, 1) < 0 && voice_of(&v, 7) < 0 && voices_on(&v) == 3);
    rain_car_voices_assign(&v, gone, 7, 103 + RAIN_CARS_REST_TICKS - 1);
    CHECK(voice_of(&v, 7) < 0);
    rain_car_voices_assign(&v, gone, 7, 103 + RAIN_CARS_REST_TICKS);
    CHECK(voice_of(&v, 7) >= 0 && v.fresh[voice_of(&v, 7)] == 1 && voices_on(&v) == RAIN_CARS_VOICES);
    rain_car_voices_assign(&v, all, 0, 200);   /* nobody heard */
    CHECK_EQ_INT(voices_on(&v), 0);
    rain_car_voices_assign(&v, all, 2, 50);   /* the clock went back (a new race): free at once */
    CHECK(voices_on(&v) == 2 && voice_of(&v, 3) >= 0 && voice_of(&v, 0) >= 0);
}

int main(void)
{
    test_dice();
    test_puddle_rate();
    test_puddle_roll();
    test_voices();
    return TEST_REPORT();
}
