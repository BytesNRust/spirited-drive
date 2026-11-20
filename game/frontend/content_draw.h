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
    int quiet;                  /* 1: no focus drawn (a box is up over the page: one focus only, the box's) */
    int flash;                  /* 1: the read-only line in yellow (a box tried on a standard edition) */
} ContentPage;

void content_draw(const ContentOps *o, const ContentPage *p);

/* the foot buttons' places by the kit's foot rule (ui_foot_layout), from the words' widths: the draw origins and the click spans, in the view */
void content_foot_layout(const ContentOps *o, ContentView *v);

#endif
