/* The first start's profile window (game/sys/first_profile.h; owner 2026-11-25): Stock, Restored, Restomod from left to right, Restored '96 the one Return and Escape
 * take, the text says Game Mode (not Options) and what each profile changes, and fits the buffer the window gives it. */
#include "game/sys/first_profile.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    Profile order[FIRST_PROFILE_COUNT];
    first_profile_order(order);
    CHECK_EQ_INT(order[0], PROFILE_STOCK);
    CHECK_EQ_INT(order[1], PROFILE_RESTORED);
    CHECK_EQ_INT(order[2], PROFILE_RESTOMOD);
    CHECK_EQ_INT(order[FIRST_PROFILE_DEFAULT], PROFILE_RESTORED);   /* the button Return and Escape take */

    char text[2000];
    const int n = first_profile_text(text, sizeof text);
    CHECK(n > 0 && (size_t)n < sizeof text);   /* nothing cut */
    CHECK(strstr(text, "Game Mode screen") != NULL);
    for (const char *line = text; *line != '\0';) {   /* Windows' message box does not wrap: no line longer than 60 characters */
        const char *end = strchr(line, '\n');
        const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        CHECK(len <= 60);
        line += len + (end != NULL);
    }
    CHECK(strstr(text, "Options") == NULL);
    CHECK(strstr(text, "keyboard, gamepad or wheel") != NULL);
    /* the profiles appear in the window's order, each with its title and what it changes */
    const char *at = text;
    for (int i = 0; i < FIRST_PROFILE_COUNT; i++) {
        const char *title = strstr(at, port_profile_title(order[i]));
        CHECK(title != NULL);
        if (title == NULL) break;
        const char *changes = first_profile_changes(order[i]);
        char flat[2000];   /* the window wraps its lines: the words are the same with the newlines as spaces */
        snprintf(flat, sizeof flat, "%s", title);
        for (char *c = flat; *c != '\0'; c++)
            if (*c == '\n') *c = ' ';
        CHECK(changes != NULL && strstr(flat, changes) != NULL);
        at = title + 1;
    }
    CHECK(strstr(first_profile_changes(PROFILE_RESTOMOD), "Everything in Restored") != NULL);
    CHECK(strstr(first_profile_changes(PROFILE_RESTOMOD), "moon") != NULL);
    CHECK(first_profile_changes((Profile)99) == NULL);
    /* a small buffer cuts the text and still ends it */
    char small[40];
    first_profile_text(small, sizeof small);
    CHECK_EQ_INT(small[sizeof small - 1], 0);
    return TEST_REPORT();
}
