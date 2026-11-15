/* dep59: the rules of a car piece (formats/car_piece.h), one for the game and the studio: the eight words of the original race models,
 * the stems an added car may take, the paths of its three files. */
#include "formats/car_piece.h"
#include "test.h"

#include <string.h>

static void test_words(void)
{
    static const char *const WORDS[] = { "supra", "diablo", "911", "zr1", "512tr", "viper", "nsx", "rx7" };
    for (int i = 0; i < 8; i++) {
        CHECK_EQ_INT(car_piece_model_of_word(WORDS[i]), i);
        CHECK(car_piece_word_of_model(i) != NULL && strcmp(car_piece_word_of_model(i), WORDS[i]) == 0);
    }
    CHECK_EQ_INT(car_piece_model_of_word("lada"), -1);
    CHECK_EQ_INT(car_piece_model_of_word("Viper"), -1);
    CHECK_EQ_INT(car_piece_model_of_word(""), -1);
    CHECK_EQ_INT(car_piece_model_of_word(NULL), -1);
    CHECK(car_piece_word_of_model(8) == NULL && car_piece_word_of_model(-1) == NULL);
}

static void test_stems(void)
{
    static const char *const OK[] = { "F40", "A1", "TESTCAR8", "M", "MX5" };
    static const char *const BAD[] = { "", "f40", "TOOLONG99", "A/B", "..", "A B", "DVIPER", "mrx7", "MRX7", "MTSUPRA", "TRAFFC", "COPMUST",
                                       "MCOPMUST", "P911T", "MP911T", "WAGON", "MJEEP", "TSUPRA", "LDIABL" };
    for (size_t i = 0; i < sizeof OK / sizeof OK[0]; i++) CHECK_EQ_INT(car_piece_stem_ok(OK[i]), 1);
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) CHECK_EQ_INT(car_piece_stem_ok(BAD[i]), 0);
    CHECK_EQ_INT(car_piece_stem_ok(NULL), 0);
}

static void test_paths(void)
{
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    car_piece_paths("F40", paths);
    CHECK(strcmp(paths[0], "SIMDATA/CARFAMS/F40.CFM") == 0);
    CHECK(strcmp(paths[1], "SIMDATA/CARFAMS/F40.PDN") == 0);
    CHECK(strcmp(paths[2], "SIMDATA/CARSPECS/F40.PBS") == 0);
}

int main(void)
{
    test_words();
    test_stems();
    test_paths();
    return TEST_REPORT();
}
