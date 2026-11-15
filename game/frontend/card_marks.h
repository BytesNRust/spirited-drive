/* port (dep56, dep64): Restomod's track card in Control Central. The card is the track's own picture, or the exe's mirror art when
 * the mirror is on (never the rally's), with words over it in the small yellow text, no box behind them:
 *  - top left, one line each, the variants that are on: MIRROR (where the mirror art's own word was, wiped), REVERSE, RALLY (in that order);
 *  - top right, ending at the card's right: the laps ("8 LAPS", "1 LAP"; a stage has none), the time of day when it is not midday
 *    (MORNING, EVENING, NIGHT: the track screen's words) and the weather when it is not clear (FOG, RAIN, STORM).
 * Stock and Restored keep the exe's card. */
#ifndef TNFS_CARD_MARKS_H
#define TNFS_CARD_MARKS_H

#include <stddef.h>
#include <stdint.h>

enum { CARD_WORDS_MAX = 3, CARD_WORD_SIZE = 12 };

typedef struct {
    int left_count, right_count;
    char left[CARD_WORDS_MAX][CARD_WORD_SIZE];
    char right[CARD_WORDS_MAX][CARD_WORD_SIZE];
} CardWords;

/* The words for `track` (0..9, the menus') as the race on it will be, from the player's choices: weather a WEATHER_* (weather.h),
 * light_time a LIGHT_TIME_* (port_settings.h) or -1 (no light effects: left out), laps the circuits' count. What the track does
 * not take is left out: a stage (0..2) has no reverse and no laps, Lost Vegas (8) and the hidden track (9) no mirror and no
 * rally, and Lost Vegas is always the night, whatever time was picked. */
CardWords card_words_of(int track, int mirror, int reverse, int rally, int weather, int light_time, int laps);

/* The mirror art's own white "Mirror" wiped: a copy of the mirror picture (a bank's bitmap entry: 16-byte header, the
 * size at 4 and 6, the place at 12 and 14, then width x height texels) whose box CARD_WORD_X0..X1 x CARD_WORD_Y0..Y1 takes the
 * texels the word covers: those of the normal picture turned left for right (the exe's mirror art is that picture turned, the
 * word and the name strip apart), lined up by the two pictures' places on the screen. Where `normal` is NULL, is not a bitmap,
 * has another width or place across, or has no row there (a mod's art), the box takes the texels of the same row CARD_WORD_W
 * further right instead (the sky beside the word, as the track screen's Night label does). The box is the word and its shadow
 * in all nine of the exe's mirror pictures and a texel round it. The caller frees it; NULL when `mirror` is not a bitmap, is
 * too small, or there is no memory. */
enum { CARD_WORD_X0 = 0, CARD_WORD_X1 = 56, CARD_WORD_Y0 = 5, CARD_WORD_Y1 = 16, CARD_WORD_W = CARD_WORD_X1 - CARD_WORD_X0 + 1 };
uint8_t *card_word_wiped(const uint8_t *mirror, const uint8_t *normal);

#endif
