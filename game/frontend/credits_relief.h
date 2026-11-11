/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the original credits pages' relief without their text, for our own credits pages (credits_pages.h). The CD holds no clean copy: credits.fsh's five
 * pages share one picture with each page's names burnt in at other places, and every other screen with that relief has a banner or text over it. So it is rebuilt
 * at run time from the player's own CD and no EA pixels live in the repository: per pixel a page with no text there (a page's text is the palette entries
 * the relief alone hardly uses, grown over the letters' edges), chosen against an estimate of the clean relief -- the configuration screens' clean relief
 * (config_bgnd.h, the same drawing, pixel for pixel) relit to the credits' light, or, where that one has its big "&" the credits do not, a smoothed medoid
 * of the five pages. The small Road & Track / TNFS SE logo at the top left is the same on every page and stays.
 * The result is in the credits' own palette, with a few of its least used entries given over to the text's colours. */
#ifndef TNFS_CREDITS_RELIEF_H
#define TNFS_CREDITS_RELIEF_H

#include <stdint.h>

enum {
    CREDITS_RELIEF_W = 640,
    CREDITS_RELIEF_H = 480,
    CREDITS_RELIEF_PAGES = 5,   /* credits.fsh's bgn1..bgn5 */
    CREDITS_RELIEF_TEXT_COLOURS = 10
};

/* the colours the pages' text needs that the credits palette lacks (TEXT_YELLOW and its ramps, cream, white), given entries of their own */
extern const uint8_t CREDITS_RELIEF_TEXT_RGB[CREDITS_RELIEF_TEXT_COLOURS][3];

/* pages: the five pages' pixels (indices into page_pal, W x H each); page_pal: their palette, display-ready RGB; menu_rgb: the configuration screens' clean
 * relief as RGB (W x H x 3). Writes the relief's pixels and its palette. 1 on success, 0 when there is no memory for the work. */
int credits_relief_compose(const uint8_t *const pages[CREDITS_RELIEF_PAGES], const uint8_t page_pal[768], const uint8_t *menu_rgb,
                           uint8_t pixels[CREDITS_RELIEF_W * CREDITS_RELIEF_H], uint8_t palette[768]);

#endif
