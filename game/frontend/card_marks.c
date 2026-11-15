/* port (dep56, dep64): see card_marks.h. */
#include "game/frontend/card_marks.h"

#include "game/race/weather.h"
#include "game/sys/port_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SHAPE_HEADER = 16, TYPE_BITMAP = 0x7b };

static void add(char words[][CARD_WORD_SIZE], int *count, const char *word)
{
    snprintf(words[*count], CARD_WORD_SIZE, "%s", word);
    (*count)++;
}

CardWords card_words_of(int mirror, int reverse, int rally, int weather, int light_time, int laps)
{
    static const char *const WEATHER[WEATHER_COUNT] = { NULL, "FOG", "RAIN", "STORM" };   /* the Weather row's words; clear says nothing */
    static const char *const TIME[4] = { NULL, "MORNING", "EVENING", "NIGHT" };            /* LIGHT_TIME_*'s, the track screen's; midday says nothing */
    CardWords w;
    memset(&w, 0, sizeof w);
    if (mirror) add(w.left, &w.left_count, "MIRROR");
    if (reverse) add(w.left, &w.left_count, "REVERSE");
    if (rally) add(w.left, &w.left_count, "RALLY");
    if (laps > 0) {
        snprintf(w.right[0], CARD_WORD_SIZE, "%d LAP%s", laps, laps == 1 ? "" : "S");
        w.right_count = 1;
    }
    if (light_time >= 0 && light_time < 4 && TIME[light_time] != NULL) add(w.right, &w.right_count, TIME[light_time]);
    if (weather >= 0 && weather < WEATHER_COUNT && WEATHER[weather] != NULL) add(w.right, &w.right_count, WEATHER[weather]);
    return w;
}

static int le16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }

/* the normal picture's row lined up with the mirror's row j, or -1 when it cannot stand in for the word's texels */
static int normal_row(const uint8_t *mirror, const uint8_t *normal, int j)
{
    if (normal == NULL || normal[0] != TYPE_BITMAP || le16(normal + 4) != le16(mirror + 4) || le16(normal + 12) != le16(mirror + 12))
        return -1;
    const int k = j + le16(mirror + 14) - le16(normal + 14);
    return k >= 0 && k < le16(normal + 6) ? k : -1;
}

uint8_t *card_word_wiped(const uint8_t *mirror, const uint8_t *normal)
{
    if (mirror == NULL || mirror[0] != TYPE_BITMAP) return NULL;
    const size_t width = (size_t)(mirror[4] | mirror[5] << 8), height = (size_t)(mirror[6] | mirror[7] << 8);
    if (width < 2 * CARD_WORD_W || height <= CARD_WORD_Y1) return NULL;
    uint8_t *out = malloc(SHAPE_HEADER + width * height);
    if (out == NULL) return NULL;
    memcpy(out, mirror, SHAPE_HEADER + width * height);
    uint8_t *px = out + SHAPE_HEADER;
    for (size_t j = CARD_WORD_Y0; j <= CARD_WORD_Y1; j++) {
        const int k = normal_row(mirror, normal, (int)j);
        const uint8_t *turned = k >= 0 ? normal + SHAPE_HEADER + (size_t)k * width : NULL;
        for (size_t i = CARD_WORD_X0; i <= CARD_WORD_X1; i++)
            px[j * width + i] = turned != NULL ? turned[width - 1 - i] : px[j * width + i + CARD_WORD_W];
    }
    return out;
}
