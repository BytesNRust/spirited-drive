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
    UI_PAGER_TEXT_NUM = 11, UI_PAGER_TEXT_DEN = 10,   /* the count's size, num/den of graver18's: its digits are 10 rows and the buttons' letters 11 */
    UI_PAGER_TEXT_Y = 439,   /* the count's line top: its last row is 456, on the background's DONE and CANCEL */
    UI_PAGER_BEARING_LEFT = 2, UI_PAGER_BEARING_RIGHT = 3   /* graver18's own blank columns at each end of a count at that size: the gaps are from the ink, not the advance */
};

/* The mask a triangle is drawn from: the solid fill, one pixel wider per row, in the middle of a UI_ARROW_PAD of empty rows and
 * columns, the room of the outline (1) and the shadow (4) text_draw_badge_mask gives it. */
enum {
    UI_ARROW_PAD = 5,
    UI_ARROW_MASK_W = UI_ARROW_INK_W + 2 * UI_ARROW_PAD, UI_ARROW_MASK_H = UI_ARROW_INK_H + 2 * UI_ARROW_PAD
};

#endif
