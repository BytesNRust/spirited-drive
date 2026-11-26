/* port (profiles): the first start's profile window text; see first_profile.h. */
#include "game/sys/first_profile.h"
#include "game/sys/dialog_text.h"

#include <stdio.h>

void first_profile_order(Profile out[FIRST_PROFILE_COUNT])
{
    out[0] = PROFILE_STOCK;
    out[1] = PROFILE_RESTORED;
    out[2] = PROFILE_RESTOMOD;
}

const char *first_profile_changes(Profile p)
{
    switch (p) {
    case PROFILE_STOCK:
        return "The 1996 game as it shipped: 4:3 picture, original menus. Only its bugs are fixed.";
    case PROFILE_RESTORED:
        return "The same game on a modern screen: widescreen and smooth motion up to 144 fps. Nothing else is changed.";
    case PROFILE_RESTOMOD:
        return "Everything in Restored, plus weather (rain, storms, fog, snow), four times of day, night lighting, cars that shine and change colour, "
               "a dashboard that lights up, mirror, reverse and rally versions of the tracks, and the lost moon track.";
    default:
        return NULL;
    }
}

int first_profile_text(char *out, size_t size)
{
    char raw[2000];
    int n = snprintf(raw, sizeof raw, "Choose how the game looks and plays.\nAll three play with keyboard, gamepad or wheel.\n"
                                "You can change it any time on the Game Mode screen.\n\n");
    Profile order[FIRST_PROFILE_COUNT];
    first_profile_order(order);
    for (int i = 0; i < FIRST_PROFILE_COUNT && n > 0 && (size_t)n < sizeof raw; i++)
        n += snprintf(raw + n, sizeof raw - (size_t)n, "%s\n%s\n%s\n\n", port_profile_title(order[i]), port_profile_line1(order[i]), first_profile_changes(order[i]));
    return dialog_wrap(raw, DIALOG_LINE_MAX, out, size);   /* Windows' message box does not wrap: a long line makes the window as wide as the screen */
}
