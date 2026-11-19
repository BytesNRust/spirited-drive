/* port: the menu UI kit's measures (docs/ui-kit.md): every pixel the kit draws with, measured on 640x480 captures of the CD's own
 * screens (tools/ui_kit_check.sh measures them again on ours and fails when one is more than a pixel off). A screen never
 * carries one of these numbers: it asks ui_kit.h. */
#ifndef TNFS_UI_KIT_METRICS_H
#define TNFS_UI_KIT_METRICS_H

/* E4, the pager "< X >" (the track select's "Road/track", the showcase's "Next car"): the triangles' cream ink is 9 x 17, each
 * a solid triangle one pixel wider per row, 8 rows up to its base; the blank columns between a triangle and the text are
 * 14 on the left and 15 on the right, and the triangles' middle row is one below the text's */
enum {
    UI_ARROW_INK_W = 9, UI_ARROW_INK_H = 17,
    UI_PAGER_GAP_LEFT = 14, UI_PAGER_GAP_RIGHT = 15,
    UI_PAGER_ARROW_BELOW_TEXT = 1,
    UI_PAGER_TEXT_BEARING = 2   /* graver18's own blank columns at each end of a count: the gaps are from the ink, not the advance */
};

/* The mask a triangle is drawn from: a solid triangle UI_ARROW_CORE_W wide and UI_ARROW_TRI_H tall, drawn in the text's soft look, which
 * darkens a pixel whose lower-right neighbour is not solid. That takes the triangle's right column (the left one) or its tip (the right
 * one), and its bottom rows, off the cream: one pixel of width and two rows of height, so the core is one wider and two taller than the
 * ink, and the right one, whose top row is not darkened, starts a row lower to show the same rows. UI_ARROW_PAD empty rows and columns
 * round it (the bottom has the shadow's two). */
enum {
    UI_ARROW_PAD = 3, UI_ARROW_PAD_BOTTOM = 2,
    UI_ARROW_CORE_W = UI_ARROW_INK_W + 1, UI_ARROW_TRI_H = UI_ARROW_INK_H + 2, UI_ARROW_CORE_H = UI_ARROW_TRI_H + 1,
    UI_ARROW_MASK_W = UI_ARROW_CORE_W + 2 * UI_ARROW_PAD,
    UI_ARROW_MASK_H = UI_ARROW_PAD + UI_ARROW_CORE_H + UI_ARROW_PAD_BOTTOM
};

#endif
