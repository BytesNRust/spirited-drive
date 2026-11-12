/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see boot_screen.h. The places are the owner's approved mockup's (i030 out7): the house and PRESENTS where TITLE.QFS has ROAD&TRACK and
 * PRESENTS, the banner where its NFS SE logo stands, the small print at the foot. */
#include "game/frontend/boot_screen.h"
#include "game/sys/game_name.h"

#include <stdio.h>

static void put(BootLine *out, int *n, BootStyle style, int y, const char *text)
{
    if (*n >= BOOT_LINES_MAX) return;
    snprintf(out[*n].text, sizeof out[*n].text, "%s", text);
    out[*n].style = style, out[*n].y = y;
    (*n)++;
}

int boot_screen_lines(const char *version, BootLine out[BOOT_LINES_MAX])
{
    int n = 0;
    put(out, &n, BOOT_HOUSE, 30, "Bytes & Rust");
    put(out, &n, BOOT_PRESENTS, 102, "PRESENTS");
    put(out, &n, BOOT_NAME, BOOT_BANNER_Y + BOOT_BANNER_H / 2, GAME_NAME);   /* the banner's middle: the drawing centres it there */
    put(out, &n, BOOT_SUBTITLE, 336, "A restomod of The Need for Speed SE");
    put(out, &n, BOOT_SMALL, 418, "Unofficial. Not affiliated with Electronic Arts");
    put(out, &n, BOOT_SMALL, 436, "The Need for Speed SE (1996) is (c) Electronic Arts.  GPL-3.0");
    if (version != NULL && version[0] != '\0') {
        char line[BOOT_LINE_CHARS];
        snprintf(line, sizeof line, "v%s", version);
        put(out, &n, BOOT_SMALL, 456, line);
    }
    return n;
}

void boot_screen_sources(int key, int buttons, int pad, HoldSource out[HOLD_SOURCES])
{
    out[HOLD_KEYS] = (HoldSource){ key != 0, HOLD_NEXT };
    out[HOLD_BUTTONS] = (HoldSource){ buttons != 0, HOLD_NEXT };
    out[HOLD_PAD] = (HoldSource){ pad != 0, HOLD_NEXT };
}
