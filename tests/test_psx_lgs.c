/* The PS1 look's light script (psx_lgs.h) on made-up text in the real files' shape: CR line endings, spaced and comma
 * separated numbers, comments, a binary trailer. No game or CD data. */
#include "game/race/psx_lgs.h"
#include "test.h"

#include <math.h>
#include <string.h>

static const char TEXT[] =
    "/* Depth ceuing definition X0,Y0, X1,Y1, X2,Y2, X3,Y3 */\r0,95  10,99  75,37  100,28\r"
    "/* Dynamic channel system (start rgb) */\r109,95,52\r/* Dynamic channel system (end rgb) */\r109,95,52,\r"
    "/* Number of light context */\r4\r/* context 1 */\r14,14  64  111,111,111  0,0,0\rCRCF\x01\x02\x03";

static void test_ambient_and_tint(void)
{
    PsxLgs lgs;
    CHECK(psx_lgs_parse((const uint8_t *)TEXT, sizeof TEXT - 1, &lgs));
    CHECK_EQ_INT(lgs.depth[0], 0);
    CHECK_EQ_INT(lgs.depth[7], 28);
    CHECK_EQ_INT(lgs.start[0], 109);
    CHECK_EQ_INT(lgs.start[2], 52);
    CHECK_EQ_INT(lgs.end[1], 95);
    float tint[3];
    psx_lgs_tint(&lgs, tint);
    CHECK(fabsf(tint[0] - 109.0f / 128.0f) < 1e-6f);
    CHECK(fabsf(tint[2] - 52.0f / 128.0f) < 1e-6f);
}

static void test_neutral_and_rejected(void)
{
    static const char NEUTRAL[] = "/* d */\n0,100, 10,95, 20,50, 100,50,\n/* s */\n128,128,128,\n/* e */\n128,128,128,\n";
    PsxLgs lgs;
    float tint[3];
    CHECK(psx_lgs_parse((const uint8_t *)NEUTRAL, sizeof NEUTRAL - 1, &lgs));
    psx_lgs_tint(&lgs, tint);
    CHECK(tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f);
    CHECK(!psx_lgs_parse((const uint8_t *)NEUTRAL, 20, &lgs));                      /* cut short */
    CHECK(!psx_lgs_parse((const uint8_t *)"/* nothing */ CRCF", 18, &lgs));        /* the trailer first */
    CHECK(!psx_lgs_parse(NULL, 10, &lgs));
    static const char BIG[] = "0,1,2,3,4,5,6,7,300,1,1,1,1,1";
    CHECK(!psx_lgs_parse((const uint8_t *)BIG, sizeof BIG - 1, &lgs));              /* a channel past 255 */
}

int main(void)
{
    test_ambient_and_tint();
    test_neutral_and_rejected();
    return TEST_REPORT();
}
