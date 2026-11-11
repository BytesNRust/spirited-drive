/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see credits_pages.h. The positions are the owner's approved mockups' (i030). */
#include "game/frontend/credits_pages.h"
#include "game/sys/game_name.h"

#include <stdio.h>

enum {
    SCREEN_CX = 320, LEFT_CX = 160, RIGHT_CX = 480,
    NAME_STEP = 17,        /* one name under the next */
    HEADER_TO_NAME = 18,   /* a header's top to its first name's */
    GROUP_GAP = 14         /* the last name of a group to the next header */
};

typedef struct {
    CreditsItem *out;
    int n;
} Page;

static void put(Page *p, CreditsKind kind, CreditsColour colour, int cx, int y, const char *text)
{
    if (p->n >= CREDITS_ITEMS_MAX) return;
    CreditsItem *const it = &p->out[p->n++];
    snprintf(it->text, sizeof it->text, "%s", text);
    it->kind = kind, it->colour = colour, it->cx = cx, it->y = y;
}

/* a header and its names, as the original pages set a group; returns where the next group's header stands */
static int group(Page *p, int cx, int y, const char *header, const char *const *names, int count)
{
    put(p, CREDITS_HEADER, CREDITS_CREAM, cx, y, header);
    y += HEADER_TO_NAME;
    for (int i = 0; i < count; i++, y += NAME_STEP) put(p, CREDITS_NAME, CREDITS_CREAM, cx, y, names[i]);
    return y + GROUP_GAP;   /* y is already a step past the last name */
}

static void homage(Page *p)
{
    put(p, CREDITS_BIG, CREDITS_YELLOW, SCREEN_CX, 112, "They built it in 1996.");
    put(p, CREDITS_BIG, CREDITS_CREAM, SCREEN_CX, 160, "We kept it running.");
    /* i055: the owner's option A, four lines, set a line and a half (42 px) lower than the three were (centred on y 310): the owner asked for room under the two big lines */
    put(p, CREDITS_LINE, CREDITS_CREAM, SCREEN_CX, 268, "For the people at EA Canada");
    put(p, CREDITS_LINE, CREDITS_CREAM, SCREEN_CX, 296, "Pioneer Productions and EA Seattle");
    put(p, CREDITS_LINE, CREDITS_CREAM, SCREEN_CX, 324, "who made it");
    put(p, CREDITS_LINE, CREDITS_CREAM, SCREEN_CX, 352, "and for everyone who still plays it");
}

static void ours(Page *p, const char *version)
{
    /* i055: Pioneer Productions made the 1994 game with EA Canada (no video credit on the original pages); EA Seattle made the PC Special Edition with EA Canada */
    static const char *const ORIGINAL[] = { "EA Canada", "Pioneer Productions", "with Road & Track", "1994-1996" };
    static const char *const SPECIAL[] = { "EA Seattle", "with EA Canada" };
    static const char *const RESTOMOD[] = { "Bytes & Rust", "and contributors" };
    static const char *const LICENCE[] = { "Code GPL-3.0-or-later", "Our data CC BY-SA 4.0" };
    static const char *const MADE_WITH[] = { "SDL3" };
    int y = group(p, LEFT_CX, 140, "Original game", ORIGINAL, 4);
    group(p, LEFT_CX, y, "Special Edition", SPECIAL, 2);
    put(p, CREDITS_TITLE, CREDITS_YELLOW, RIGHT_CX, 76, GAME_NAME);
    y = group(p, RIGHT_CX, 120, "Restomod", RESTOMOD, 2);
    y = group(p, RIGHT_CX, y, "Licence", LICENCE, 2);
    group(p, RIGHT_CX, y, "Made with", MADE_WITH, 1);
    put(p, CREDITS_NOTE, CREDITS_CREAM, SCREEN_CX, 418, "Unofficial. Not affiliated with Electronic Arts");
    put(p, CREDITS_NOTE, CREDITS_CREAM, SCREEN_CX, 436, "The Need for Speed SE (1996) is (c) Electronic Arts.  GPL-3.0");
    if (version != NULL && version[0] != '\0') {
        char line[CREDITS_ITEM_CHARS];
        snprintf(line, sizeof line, "v%s", version);
        put(p, CREDITS_NOTE, CREDITS_CREAM, SCREEN_CX, 456, line);
    }
}

int credits_page(int page, const char *version, CreditsItem out[CREDITS_ITEMS_MAX])
{
    Page p = { out, 0 };
    if (page == 0)
        homage(&p);
    else if (page == 1)
        ours(&p, version);
    return p.n;
}
