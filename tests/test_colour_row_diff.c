#include "colour_row_diff.h"
#include "test.h"

#include <string.h>

int main(void)
{
    char out[1024];
    /* the Diablo: two colors in the repo's row, eight on the card; the first one's value changed */
    colour_row_diff("Rosso Diablo #b3121e, 30th Anniversary Lavender #9878c8",
                    "Rosso Diablo #8a0600, 30th Anniversary Lavender #b57edc, Giallo Orion #eba628, Bianco #e1e3db", out, sizeof out);
    CHECK(strstr(out, "  colors: 2 -> 4 (the game's table row goes to 4)\n") != NULL);
    CHECK(strstr(out, "  changed: Rosso Diablo #b3121e -> #8a0600; 30th Anniversary Lavender #9878c8 -> #b57edc\n") != NULL);
    CHECK(strstr(out, "  added: Giallo Orion #eba628, Bianco #e1e3db\n") != NULL);
    CHECK(strstr(out, "removed") == NULL);
    /* the same number: one value and one name changed */
    colour_row_diff("A #101010, B #202020, C #303030", "A #101010, Bee #202020, C #313030", out, sizeof out);
    CHECK(strstr(out, "  colors: 3 (the same number)\n") != NULL);
    CHECK(strstr(out, "  changed: B #202020 -> Bee #202020; C #303030 -> #313030\n") != NULL);
    CHECK(strstr(out, "added") == NULL && strstr(out, "removed") == NULL);
    /* fewer: the last ones are taken away */
    colour_row_diff("A #101010, B #202020, C #303030", "A #101010", out, sizeof out);
    CHECK(strstr(out, "  colors: 3 -> 1 (the game's table row goes to 1)\n") != NULL);
    CHECK(strstr(out, "  removed: B #202020, C #303030\n") != NULL && strstr(out, "changed") == NULL);
    /* nothing but the count line when the rows are alike; a small buffer is cut and closed */
    colour_row_diff("A #101010", "A #101010", out, sizeof out);
    CHECK_EQ_INT((int)strlen(out), (int)strlen("  colors: 1 (the same number)\n"));
    char small[20];
    CHECK(colour_row_diff("A #101010, B #202020", "A #101010", small, sizeof small) < sizeof small);
    CHECK(strlen(small) < sizeof small);
    return TEST_REPORT();
}
