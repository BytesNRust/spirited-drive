/* port (dep59, Lunar Springs' entry): Lunar Springs' card in Control Central. The CD's moon card (FRONTEND/GART/CONTROL/MOON.QFS) names
 * the place "LUNAR" in the small red letters of the other cards; the entry is Lunar Springs, so "SPRINGS" is added after it, taken from
 * Rusty Springs' rally card in the game's language (springsr.qfs: the same letters, red with a black shadow, in the same place in the
 * card's picture but not on the same rows in every language).
 *
 * The letters are found by their colour (the screen's palette: the red the labels are written in, the black of their shadow), not
 * measured, and written in the moon label's own red. Nothing is kept: Control Central makes the card at each draw and frees it. */
#ifndef TNFS_LUNAR_CARD_H
#define TNFS_LUNAR_CARD_H

#include <stdint.h>

/* `moon` and `rusty` are the cards' bitmap entries (a bank's: 16-byte header, the size at 4 and 6, then width x height texels, in the
 * screen's palette `pal`, 768 bytes of 8-bit RGB). A copy of `moon` with the last word of `rusty`'s label written after its own label,
 * its baseline on the moon label's, the gap the two words have on `rusty` between them. The caller frees it. NULL when either is not a
 * bitmap, the two differ in size, no label is found in one (a mod's art), the second word of `rusty`'s is not there or does not fit
 * the width, or there is no memory; the card is then shown as it is. */
uint8_t *lunar_card_named(const uint8_t *moon, const uint8_t *rusty, const uint8_t *pal);

#endif
