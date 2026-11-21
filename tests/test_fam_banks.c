/* dep96 step 1 spike: a built FAM read back (formats/fam_banks.h) on Studio's generic art: its banks, their letters and sizes, a picture as RGBA with the
 * see-through pixels clear; refusals by name. */
#include "formats/fam_banks.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *load(size_t *size)
{
    FILE *f = fopen(TNFS_SOURCE_DIR "/editor/art/GENERIC.FAM", "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(*size);
    if (data != NULL && fread(data, 1, *size, f) != *size) free(data), data = NULL;
    fclose(f);
    return data;
}

int main(void)
{
    size_t size = 0;
    uint8_t *fam = load(&size);
    CHECK(fam != NULL);
    if (fam == NULL) return TEST_REPORT();
    ExchangeError err;
    uint32_t banks = 0;
    CHECK(fam_terrain_banks(fam, size, &banks, &err));
    CHECK(banks >= 13);   /* 0-3 road, gravel, rock, grass; 4 the tarmac shoulder; 6 two lanes; 10-12 the fences */
    FamBankInfo info;
    CHECK(fam_bank_info(fam, size, 3, &info, &err));
    CHECK(info.letters & FAM_LETTER_A);
    CHECK(info.width[0] > 0 && info.height[0] > 0);
    CHECK(fam_bank_info(fam, size, 10, &info, &err) && (info.letters & FAM_LETTER_C) && info.width[2] > 0);   /* the fence's letter */
    CHECK(!fam_bank_info(fam, size, banks, &info, &err));   /* one past the last */
    CHECK(strstr(err.message, "terrain bank") != NULL);

    uint8_t *rgba = NULL;
    int w = 0, h = 0;
    CHECK(fam_bank_rgba(fam, size, 3, 'A', &rgba, &w, &h, &err));   /* the grass: solid, with colour */
    if (rgba != NULL) {
        int clear = 0, colour = 0;
        for (int i = 0; i < w * h; ++i) {
            clear += rgba[4 * i + 3] == 0;
            colour += rgba[4 * i] != rgba[4 * i + 1] || rgba[4 * i + 1] != rgba[4 * i + 2];
        }
        CHECK(clear == 0 && colour > 0);
        free(rgba);
    }
    CHECK(fam_bank_rgba(fam, size, 11, 'C', &rgba, &w, &h, &err));   /* the rail is see-through between its boards */
    if (rgba != NULL) {
        int clear = 0;
        for (int i = 0; i < w * h; ++i) clear += rgba[4 * i + 3] == 0;
        CHECK(clear > 0 && clear < w * h);
        free(rgba);
    }
    CHECK(!fam_bank_rgba(fam, size, 3, 'D', &rgba, &w, &h, &err) && rgba == NULL);
    CHECK(!fam_bank_rgba(fam, size, banks + 5, 'A', &rgba, &w, &h, &err));
    CHECK(!fam_terrain_banks((const uint8_t *)"not a fam at all", 16, &banks, &err));
    CHECK(strstr(err.message, "not a track art file") != NULL);
    free(fam);
    return TEST_REPORT();
}
