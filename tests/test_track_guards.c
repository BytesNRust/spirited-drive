/* dep59 step 5, T7a: the original's record checks and the screens' image read and write no block past the nine tracks'.
 * An added track (menu number 16..79) and a stray number reach them from --drive; under ASan a write past the image fails here. */
#include "game/frontend/record_check.h"
#include "game/frontend/records_block.h"
#include "test.h"

#include <string.h>

enum { TRACK_WORD = 0x95, CAR_WORD = 0x9b };

static void put16(uint8_t *p, int v) { p[0] = (uint8_t)v, p[1] = (uint8_t)((unsigned)v >> 8); }

static void test_block_known(void)
{
    CHECK(records_block_known(0) && records_block_known(8));
    CHECK(!records_block_known(-1) && !records_block_known(9) && !records_block_known(16) && !records_block_known(79));
    static uint8_t image[RECORDS_BLOCK_SIZE * 9];
    CHECK(records_block_at(image, 3) == image + RECORDS_BLOCK_SIZE * 3);
    const uint8_t *none = records_block_at(image, 79);
    CHECK(none != NULL && none[0] == 0 && none[RECORDS_BLOCK_SIZE - 1] == 0);
}

/* the checks on a track of no block: nothing beaten, nothing written, the image as it was */
static void test_checks_write_nothing(void)
{
    static const int TRACKS[] = { -1, 9, 16, 79 };
    static uint8_t game[RECORD_CHECK_GAME], image[RECORD_CHECK_IMAGE], before[RECORD_CHECK_IMAGE];
    static const int32_t times[RECORD_CHECK_TIMES] = { 1, 1, 1, 1, 1, 1, 1 };
    for (size_t k = 0; k < sizeof TRACKS / sizeof TRACKS[0]; k++) {
        memset(game, 0x11, sizeof game), memset(image, 0, sizeof image);
        put16(image + TRACK_WORD, TRACKS[k]), put16(image + CAR_WORD, 2);
        memcpy(before, image, sizeof image);
        CHECK(record_check(image, game, 0) == 0);
        CHECK(record_check_lap(image, game, 0) == 0);
        CHECK(record_check_top(image, game, 0) == 0);
        CHECK(record_check_totals(image, game, 0, times) == RECORD_CHECK_NONE);
        CHECK(memcmp(image, before, sizeof image) == 0);
    }
}

int main(void)
{
    test_block_known();
    test_checks_write_nothing();
    return TEST_REPORT();
}
