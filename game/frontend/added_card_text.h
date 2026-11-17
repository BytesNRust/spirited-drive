/* port (section 1 of notes/dep59-added-gallery-showcase-plan.md): an added car's name made the letters added_card.h draws over its
 * card: GRAVER18 (the game's main face, loaded by text_init) at ADDED_CARD_NAME_NUM / DEN, fitted to the card's width with "..." when
 * it is too long (added_card_name_fit). Made in memory each time the card is: nothing of the font goes to a file. */
#ifndef TNFS_ADDED_CARD_TEXT_H
#define TNFS_ADDED_CARD_TEXT_H

#include "game/frontend/added_card.h"

/* `name`'s letters for the card (`rival` 0) or the rival card (1): `out` points into `*mask` (malloc'd, the caller frees it), the text
 * drawn in `text` (`text_size` bytes); 0 when nothing is drawn (no letter the font has, no font, no memory) */
int added_card_name_letters(const char *name, int rival, AddedCardName *out, uint8_t **mask, char *text, int text_size);

/* the same for an added track's card (ADDED_TRACK_W wide) */
int added_track_card_name_letters(const char *name, AddedCardName *out, uint8_t **mask, char *text, int text_size);

#endif
