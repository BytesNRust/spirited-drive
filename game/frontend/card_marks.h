/* port (dep56): the words at the top of Control Central's track card in Restomod, so the card says what the track screen set:
 * the layout's letters (M the mirror, I inverse, R the rally, those that are on), the weather, the time of day (with the light
 * effects; -1 leaves it out) and the laps (0 leaves them out: a stage has none). The same short words as Best Times' conditions. */
#ifndef TNFS_CARD_MARKS_H
#define TNFS_CARD_MARKS_H

#include <stddef.h>

void card_marks_text(int mirror, int inverse, int rally, int weather, int light_time, int laps, char *out, size_t size);

#endif
