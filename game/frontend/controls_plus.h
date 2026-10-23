/* port: the controls screen's additions (not in the original): two more pages for the pad's and wheel's own bindings
 * (controls.cfg, port_controls.h), the dead zone, the sensitivity and the force feedback's strength. Drawn with the game's
 * text routine over the screen's own art; controls_menu.c calls in only when its state has a `plus`. */
#ifndef TNFS_CONTROLS_PLUS_H
#define TNFS_CONTROLS_PLUS_H

#include "game/frontend/controls_menu.h"

/* the third page's rows */
enum { TUNE_DEADZONE, TUNE_SENSITIVITY, TUNE_FORCE, TUNE_COUNT };

/* the page indicator at the bottom centre ("1/3 >", "< 2/3 >", "< 3/3"): a click on its left half turns back, on its
 * right half forward (pointer_in_rect's order) */
enum { CONTROLS_NAV_X0 = 270, CONTROLS_NAV_X1 = 370, CONTROLS_NAV_Y0 = 432, CONTROLS_NAV_Y1 = 476 };

/* a tuning value one step (5) up (1) or down (-1), kept within its range (no wrap); 1 when it moved */
int controls_plus_tune(PortControls *c, int which, int direction);
void controls_plus_tune_text(const PortControls *c, int which, char out[16]);

/* the page indicator of page 1 (0), 2 (1) or 3 (2): its count and a triangle to each side with a page to go to, the
 * `lit` one (1 left, 2 right, 0 none: controls_plus_nav_lit's) yellow */
void controls_plus_nav(const ControlsMenuOps *o, int page, int lit);
/* the triangle the pointer is on */
int controls_plus_nav_lit(const ControlsMenuOps *o, int page);
/* 1 when the pointer is on the indicator, 0 off it */
int controls_plus_nav_hit(const ControlsMenuOps *o);

typedef enum { CONTROLS_PAGE_DONE, CONTROLS_PAGE_CANCEL, CONTROLS_PAGE_BACK } ControlsPageResult;
/* The pages 2 and 3, run to their end over the screen's `bank` and `rgb` palette: Done or the idle deadline leave
 * with the controls as they are, Cancel and Escape say so (the caller puts them back), Left on page 2 or a click on the
 * indicator's left half go back to page 1. */
ControlsPageResult controls_plus_run(const ControlsMenuState *st, const ControlsMenuOps *o, int32_t bank,
                                     const uint8_t *rgb);

#endif
