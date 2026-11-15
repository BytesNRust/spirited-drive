/* See car_piece.h. */
#include "formats/car_piece.h"

#include "formats/mod_ini.h"
#include "formats/slots.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char *const WORDS[] = { "supra", "diablo", "911", "zr1", "512tr", "viper", "nsx", "rx7" };
enum { WORD_COUNT = sizeof WORDS / sizeof WORDS[0] };

/* a root the race loads by that slots.h does not list: car_load.c's car_model_names[10] */
static const char *const RACE_ONLY_STEMS[] = { "P911T", NULL };

int car_piece_model_of_word(const char *word)
{
    if (word == NULL) return -1;
    for (int i = 0; i < WORD_COUNT; i++)
        if (strcmp(word, WORDS[i]) == 0) return i;
    return -1;
}

const char *car_piece_word_of_model(int model)
{
    return model >= 0 && model < WORD_COUNT ? WORDS[model] : NULL;
}

/* 1 when `stem` is `own` or 'M' + `own`, whatever the case */
static int names(const char *stem, const char *own)
{
    return strcasecmp(stem, own) == 0 || ((stem[0] == 'M' || stem[0] == 'm') && strcasecmp(stem + 1, own) == 0);
}

int car_piece_stem_ok(const char *stem)
{
    if (stem == NULL) return 0;
    const size_t n = strlen(stem);
    if (n == 0 || n > MOD_INI_STEM_MAX) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((stem[i] >= 'A' && stem[i] <= 'Z') || (stem[i] >= '0' && stem[i] <= '9'))) return 0;
    for (const char *const *s = slot_stems(SLOT_CAR); *s != NULL; s++)
        if (names(stem, *s)) return 0;
    for (const char *const *s = RACE_ONLY_STEMS; *s != NULL; s++)
        if (names(stem, *s)) return 0;
    return 1;
}

void car_piece_paths(const char *stem, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH])
{
    snprintf(paths[0], CAR_PIECE_PATH, "SIMDATA/CARFAMS/%s.CFM", stem);
    snprintf(paths[1], CAR_PIECE_PATH, "SIMDATA/CARFAMS/%s.PDN", stem);
    snprintf(paths[2], CAR_PIECE_PATH, "SIMDATA/CARSPECS/%s.PBS", stem);
}
