/* port (dep59 step 3): an added car's card and rival card for Control Central, from its piece's pictures (notes/dep59-step3-card-
 * gallery-plan.md, Task 8): card.png (309x166) where the exe's card is, and rival.png (284x117) where its rival card is, or card.png's
 * middle when the piece has no rival.png. Each picture is put in the screen's palette (picture_palette.h) and made a shape bank whose
 * "0000" is it at its place, as the CD's control/<car>1.qfs and <car>2.qfs are. The game does not scale: a picture of another size is
 * refused.
 * Section 1 of notes/dep59-added-gallery-showcase-plan.md: the car's name (mod.ini `name`) drawn over the picture as the CD's cards
 * carry theirs, measured on them: red (238, 0, 0), a black shadow 1 px down-right, from x 4, the capitals ending on row 159 of the card
 * (110 of the rival card), in GRAVER18 at 3/4 (added_card_text.c makes the letters; nothing of the font is written to a file). */
#ifndef TNFS_ADDED_CARD_H
#define TNFS_ADDED_CARD_H

#include "formats/png.h"

#include <stddef.h>
#include <stdint.h>

enum {
    ADDED_CARD_W = 309, ADDED_CARD_H = 166, ADDED_CARD_X = 21, ADDED_CARD_Y = 114,      /* the CD's car cards' "0000" */
    ADDED_RIVAL_W = 284, ADDED_RIVAL_H = 117, ADDED_RIVAL_X = 341, ADDED_RIVAL_Y = 99   /* its rival cards' */
};

/* port (dep59 step 5, T7c): an added track's card, in the track's slot of Control Central: 208x127 at (341, 255) (the moon card's), its
 * name's capitals ending on row 120 from column 4 as "Rusty Springs" has them (measured on Control Central, 2026-11-18, the owner's
 * comparison; ADDED_TRACK_NAME_X is the letters' box's first column, a D's stem one in); card.png (that size) or, without one, a flat card the game makes */
enum { ADDED_TRACK_W = 208, ADDED_TRACK_H = 127, ADDED_TRACK_X = 341, ADDED_TRACK_Y = 255, ADDED_TRACK_NAME_BASE = 120, ADDED_TRACK_NAME_X = 3 };

enum {
    ADDED_CARD_NAME_X = 4,          /* the names' first column on the CD's cards (3..5) */
    ADDED_CARD_NAME_BASE = 159,     /* their capitals' last row on a card (151..159) */
    ADDED_RIVAL_NAME_BASE = 110,    /* on a rival card (101..110) */
    ADDED_CARD_NAME_NUM = 3, ADDED_CARD_NAME_DEN = 4   /* GRAVER18's size */
};

/* a name's letters: coverage 0..15 a pixel, w x h, its capitals' last row `base_row` */
typedef struct {
    const uint8_t *mask;
    int w, h, base_row;
} AddedCardName;

/* `img` (RGBA, alpha ignored, or indexed through its own palette) as the card (`rival` 0) or the rival card (1; `from_card` 1: `img` is
 * card.png and its middle is taken), each pixel the nearest of `palette`'s entries `usable` marks: a malloc'd shape bank and its size;
 * NULL with the reason in `why` when the picture is not its size or memory ran out */
uint8_t *added_card_bank(const PngImage *img, int rival, int from_card, const uint8_t palette[768], const uint8_t usable[256], size_t *size,
                         char *why, size_t why_cap);
/* the same with `name` drawn over it (NULL: none) */
uint8_t *added_card_bank_named(const PngImage *img, int rival, int from_card, const AddedCardName *name, const uint8_t palette[768],
                               const uint8_t usable[256], size_t *size, char *why, size_t why_cap);

/* the track card as the track slot's bank: `img` (card.png, ADDED_TRACK_W x ADDED_TRACK_H) or, when NULL, a flat dark card with a frame, its
 * `name` drawn over it (NULL: none); NULL with the reason in `why` when the picture is not its size or memory ran out */
uint8_t *added_track_card_bank(const PngImage *img, const AddedCardName *name, const uint8_t palette[768], const uint8_t usable[256],
                               size_t *size, char *why, size_t why_cap);

/* the same card as RGBA (ADDED_TRACK_W x ADDED_TRACK_H x 4), malloc'd, for a page that shows it in a palette of its own (track_page_glue.c);
 * NULL with the reason in `why` */
uint8_t *added_track_card_rgba(const PngImage *img, const AddedCardName *name, char *why, size_t why_cap);

/* `name` drawn on `rgba` (w x h, alpha kept): its shadow, then its letters mixed with the picture by coverage, from ADDED_CARD_NAME_X,
 * its capitals' last row on `base`; what falls outside cut */
void added_card_draw_name(uint8_t *rgba, int w, int h, const AddedCardName *name, int base);
/* the same from column `x0` (a track card's names start at ADDED_TRACK_NAME_X) */
void added_card_draw_name_at(uint8_t *rgba, int w, int h, const AddedCardName *name, int base, int x0);

/* `name` as the card draws it: text_words' (the bytes GRAVER18 draws right), its spaces trimmed, and when `measure` (a run's width in
 * pixels) says it is wider than `max_w`, cut at the end with "..." (no space before it); in `out` (`size` bytes, cutting too). 0 when
 * nothing is left to draw */
int added_card_name_fit(const char *name, int max_w, int (*measure)(void *ctx, const char *s), void *ctx, char *out, int size);

#endif
