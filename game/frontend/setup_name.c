/* See setup_name.h. */
#include "game/frontend/setup_name.h"

#include "game/frontend/gallery_layout.h"
#include "game/sys/text.h"

#include <stdio.h>
#include <string.h>

static const int SIZES[3][2] = { { GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN }, { 3, 4 }, { 5, 8 } };

int setup_name_fit(const char *value, int room, char *out, int size, int *num, int *den)
{
    snprintf(out, (size_t)size, "%s", value);
    for (int k = 0; k < 3; k++) {
        *num = SIZES[k][0], *den = SIZES[k][1];
        if (text_width_scaled(out, *num, *den) <= room) break;
    }
    while (out[0] != 0 && text_width_scaled(out, *num, *den) > room) {   /* past the smallest: cut, a "." at the end */
        const size_t n = strlen(out);
        out[n - 1] = 0;
        if (n > 1) out[n - 2] = '.';
    }
    return text_width_scaled(out, *num, *den);
}

void setup_name_draw(const char *value, int x0, int x1, int mid)
{
    char fits[64];
    int num, den, top, bottom;
    const int w = setup_name_fit(value, x1 - x0, fits, sizeof fits, &num, &den);
    text_ink_scaled("H", num, den, &top, &bottom);
    text_draw_smooth(fits, x0 + (x1 - x0 - w) / 2, mid - (top + bottom) / 2, num, den);
}
