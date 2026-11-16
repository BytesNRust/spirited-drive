/* port (dep59 step 4b, T5): the CONTENT screen's page drawn (content_screen.h), the mock-up's places in content_nav.h: what the loop
 * (content_screen.c) hands it is all it reads. */
#ifndef TNFS_CONTENT_DRAW_H
#define TNFS_CONTENT_DRAW_H

#include "game/frontend/content_screen.h"

typedef struct {
    const char *edition;        /* the selector's name */
    const ContentMods *mods;    /* for the card's words */
    const ContentView *view;    /* the rows, the buttons that can be taken */
    const ContentNav *nav;      /* the selection, the tab, the page */
} ContentPage;

void content_draw(const ContentOps *o, const ContentPage *p);

#endif
