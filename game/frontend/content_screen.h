/* port (dep59 step 4b, T5): the CONTENT screen ("Manage content..." in Game Mode; notes/dep59-step4b-content-screen-plan.md, the design's
 * "Pantalla CONTENT"): the edition being edited on Game Mode's selector, its pieces by tab (content_rows.h) with the selection
 * (content_nav.h), the card of the row selected, and the foot's buttons. Every edit is saved at once, as Customize Game's; Done leaves
 * once the edition has a car and a track it can race (the rule), on the edition the selector shows. Drawn in the game's text over
 * Customize Game's clean background; the files, the dialogs and the pictures are the caller's (ContentOps). Logs mark:screen=content as
 * it opens and mark:content=<tab> <at> <line> <item> <page> at each draw, for the walks. */
#ifndef TNFS_CONTENT_SCREEN_H
#define TNFS_CONTENT_SCREEN_H

#include "game/frontend/content_nav.h"

#include <stddef.h>
#include <stdint.h>

/* the text styles (text.h's TextStyle) */
enum { CONTENT_CREAM = 0, CONTENT_YELLOW = 1, CONTENT_RED = 2, CONTENT_DIM = 4 };
/* the mock-up's flat colours, 0xRRGGBB: the rules and the preview's frame, the preview's panel */
enum { CONTENT_RULE_RGB = 0x7a654a, CONTENT_PANEL_RGB = 0x1a140d };
/* the arrows ContentOps.arrow draws */
enum { CONTENT_ARROW_UP, CONTENT_ARROW_DOWN, CONTENT_ARROW_LEFT, CONTENT_ARROW_RIGHT };

typedef struct ContentOps {
    /* the page, drawn whole at each change: Customize Game's clean background, the game's text at num/den of its size (style:
     * CONTENT_*), the title in the original screens' look, a flat rectangle (x0..x1, y0..y1 inclusive), a small triangle (lit yellow,
     * else cream) with its top left at x, y, the piece's picture fitted into the box, then the page on the display */
    void (*background)(void *ctx);
    void (*text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den);
    int32_t (*text_width)(void *ctx, const char *text, int32_t num, int32_t den);
    void (*title)(void *ctx, const char *text, int32_t x, int32_t y);
    void (*fill)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rgb);
    void (*arrow)(void *ctx, int32_t which, int32_t lit, int32_t x, int32_t y);
    void (*preview)(void *ctx, int32_t tab, const char *id, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    void (*show)(void *ctx);
    /* the player: a key (0 none), the pointer's place and buttons (bit 0 the left), and a frame's wait (the music fed) */
    int32_t (*key)(void *ctx);
    int32_t (*pointer)(void *ctx, int32_t *x, int32_t *y);
    void (*wait)(void *ctx);
    /* the editions on the selector: how many (0 the standard edition, the player's own after it), one's name, one read (1; 0 for the
     * standard edition, which has no file), one saved (1 when written) */
    int32_t (*editions)(void *ctx);
    void (*edition_name)(void *ctx, int32_t index, char *out, size_t cap);
    int32_t (*edition_load)(void *ctx, int32_t index, Edition *out);
    int32_t (*edition_save)(void *ctx, int32_t index, const Edition *e);
    /* the mods the rows are built from (the arrays the caller's, kept until the next call), and in how many editions `id` is */
    void (*mods)(void *ctx, ContentMods *out);
    int32_t (*used_in)(void *ctx, const char *id);
    /* a card's button (CONTENT_CARD_*) for the piece `id` of `tab`, and a foot's (CONTENT_FOOT_*, not Done) with the edition shown in
     * `edition` (set to the one to show after it): each runs its dialog and its work; 1 when the editions or the mods changed */
    int32_t (*card)(void *ctx, int32_t button, int32_t tab, const char *id);
    int32_t (*foot)(void *ctx, int32_t button, int32_t *edition);
    /* Done refused: the edition lacks what CONTENT_RULES_* `lacks` says; the player is told */
    void (*refused)(void *ctx, int32_t lacks);
    void *ctx;
    /* port (T7): a file dropped on the window since the last turn, installed by the caller with its dialog (NULL: none taken); 1 when the
     * editions or the mods changed, with `edition` set to the one to show */
    int32_t (*dropped)(void *ctx, int32_t *edition);
    /* port: the first and last columns the text's ink covers at num/den from its pen's x 0, 1 when it has ink (NULL: the whole width is ink) */
    int32_t (*text_ink)(void *ctx, const char *text, int32_t num, int32_t den, int32_t *left, int32_t *right);
} ContentOps;

/* the screen until Done (or Escape) with the rule kept; `edition` the selector's choice it opens on, and the one it closed on */
void content_screen_run(const ContentOps *o, int32_t *edition);

#endif
