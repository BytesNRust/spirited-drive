/* Section 1 of notes/dep59-added-gallery-showcase-plan.md: an added car's name in GRAVER18 at 3/4 over its card as the CD's cards carry
 * theirs (game/frontend/added_card_text.h). With the CD's font: "Porsche 911 CARRERA" drawn where the CD's 911 card has it (from x 4,
 * the capitals ending on row 159, 110 on the rival card, about as wide as the CD's 210 px); a name too long for the card cut with
 * "..." within it; a name with nothing the font draws, none. Skipped without original/. */
#include "game/frontend/added_card.h"
#include "game/frontend/added_card_text.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* the red's box on a black w x h card the name was drawn on */
static void red_box(const uint8_t *rgba, int w, int h, int *x0, int *x1, int *y0, int *y1)
{
    *x0 = w, *x1 = -1, *y0 = h, *y1 = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t *p = rgba + 4 * ((size_t)y * w + x);
            if (p[0] > 180 && p[1] < 60) {
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y < *y0) *y0 = y;
                if (y > *y1) *y1 = y;
            }
        }
}

static void drawn(const char *name, int rival, char *text, int *x0, int *x1, int *y0, int *y1, int *ok)
{
    const int w = rival ? ADDED_RIVAL_W : ADDED_CARD_W, h = rival ? ADDED_RIVAL_H : ADDED_CARD_H;
    uint8_t *rgba = calloc((size_t)w * h, 4), *mask = NULL;
    AddedCardName n;
    *ok = added_card_name_letters(name, rival, &n, &mask, text, 64);
    if (*ok) added_card_draw_name(rgba, w, h, &n, rival ? ADDED_RIVAL_NAME_BASE : ADDED_CARD_NAME_BASE);
    red_box(rgba, w, h, x0, x1, y0, y1);
    free(rgba), free(mask);
}

int main(void)
{
    char cd[512], probe[600];
    snprintf(cd, sizeof cd, "%s/original", TNFS_SOURCE_DIR);
    snprintf(probe, sizeof probe, "%s/FRONTEND/ART/FONTS/GRAVER18.FFN", cd);
    if (access(probe, R_OK) != 0) {
        printf("skip (no original/)\n");
        return TEST_REPORT();
    }
    resource_init(cd, 0);
    CHECK(text_init());
    char text[64];
    int x0, x1, y0, y1, ok;
    drawn("Porsche 911 CARRERA", 0, text, &x0, &x1, &y0, &y1, &ok);   /* the CD's: x 5..214, capitals 151..159 */
    CHECK(ok && strcmp(text, "Porsche 911 CARRERA") == 0);
    CHECK(x0 >= 4 && x0 <= 6);
    CHECK_EQ_INT(y1, ADDED_CARD_NAME_BASE);
    CHECK(x1 - x0 >= 195 && x1 - x0 <= 215);
    printf("Porsche 911 CARRERA on the card: x %d..%d, y %d..%d\n", x0, x1, y0, y1);
    drawn("Lamborghini DIABLO VT", 1, text, &x0, &x1, &y0, &y1, &ok);   /* the CD's rival card: x 4..224, 102..110 */
    CHECK(ok && x0 >= 4 && x0 <= 6 && y1 == ADDED_RIVAL_NAME_BASE);
    printf("Lamborghini DIABLO VT on the rival card: x %d..%d, y %d..%d\n", x0, x1, y0, y1);
    /* too long: cut with "...", within the card */
    const char *longest = "The Quite Unbelievably Long Name of a Super Car X";   /* 48 */
    drawn(longest, 0, text, &x0, &x1, &y0, &y1, &ok);
    CHECK(ok && strlen(text) > 3 && strcmp(text + strlen(text) - 3, "...") == 0 && x1 < ADDED_CARD_W - ADDED_CARD_NAME_X);
    drawn(longest, 1, text, &x0, &x1, &y0, &y1, &ok);
    CHECK(ok && strcmp(text + strlen(text) - 3, "...") == 0 && x1 < ADDED_RIVAL_W - ADDED_CARD_NAME_X);
    printf("the long name on the rival card: \"%s\", to x %d\n", text, x1);
    drawn("\xc3\xa9\xc3\xa9", 0, text, &x0, &x1, &y0, &y1, &ok);   /* nothing the face draws: no name */
    CHECK(!ok && x1 < 0);
    return TEST_REPORT();
}
