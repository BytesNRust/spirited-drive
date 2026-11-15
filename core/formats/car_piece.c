/* See car_piece.h. */
#include "formats/car_piece.h"

#include "formats/mod_ini.h"
#include "formats/pbs.h"
#include "formats/slots.h"

#include <ctype.h>

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

/* the file of `path` in `files`, as it is, in upper case or in lower case (the overlay's rule, docs/modding.md) */
static const SetFile *find_any_case(const FileSet *files, const char *path)
{
    const SetFile *f = fileset_find(files, path);
    char other[CAR_PIECE_PATH];
    for (int lower = 0; f == NULL && lower < 2; lower++) {
        size_t i = 0;
        for (; path[i] != '\0' && i + 1 < sizeof other; i++)
            other[i] = (char)(lower ? tolower((unsigned char)path[i]) : toupper((unsigned char)path[i]));
        other[i] = '\0';
        f = fileset_find(files, other);
    }
    return f;
}

/* PIECE.FROM: `word` (key `key`) names an original car, or is not given and need not be */
static void from_ok(const char *key, const char *word, int required, Report *r)
{
    if (word[0] == '\0') {
        if (required) report_add(r, "PIECE.FROM", 0, "mod.ini: no %s", key);
        return;
    }
    if (car_piece_model_of_word(word) < 0)
        report_add(r, "PIECE.FROM", 0, "mod.ini: %s=%s is not supra, diablo, 911, zr1, 512tr, viper, nsx or rx7", key, word);
}

static int32_t le32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

/* PIECE.SPECS: what the race divides by or walks, and the checksum without which the exe zeroes the power */
static void specs_ok(const SetFile *f, const char *path, Report *r)
{
    uint8_t rec[PBS_SIZE];
    if (!pbs_read(rec, f->data, f->size)) return;   /* validate_file said why */
    if (!pbs_checksum_ok(rec)) report_add(r, "PIECE.SPECS", 0, "%s: the checksum does not match (the game would zero its power)", path);
    const int32_t power = le32(rec + CAR_PIECE_PBS_POWER), points = le32(rec + CAR_PIECE_PBS_TORQUE_POINTS);
    if (power <= 0) report_add(r, "PIECE.SPECS", power, "%s: power is not above 0", path);
    if (le32(rec + CAR_PIECE_PBS_UNKNOWN_320) == 0) report_add(r, "PIECE.SPECS", 0, "%s: unknown_320 is 0", path);
    if (le32(rec + CAR_PIECE_PBS_FORCE_SCALE) == 0) report_add(r, "PIECE.SPECS", 0, "%s: force_scale is 0", path);
    if (points < CAR_PIECE_TORQUE_POINTS_MIN || points > CAR_PIECE_TORQUE_POINTS_MAX)
        report_add(r, "PIECE.SPECS", points, "%s: torque_points is not %d..%d", path, CAR_PIECE_TORQUE_POINTS_MIN, CAR_PIECE_TORQUE_POINTS_MAX);
}

int car_piece_check(const ModInfo *card, const FileSet *files, Report *r)
{
    const size_t fatal_before = report_fatal_count(r);
    if (card->type != MOD_TYPE_CAR) report_add(r, "PIECE.TYPE", card->type, "mod.ini: type is not car");
    from_ok("sound_from", card->sound_from, 1, r);
    from_ok("ai_from", card->ai_from, 1, r);
    from_ok("dash_from", card->dash_from, 0, r);
    if (!car_piece_stem_ok(card->stem)) {
        report_add(r, "PIECE.STEM", (int64_t)strlen(card->stem), "mod.ini: stem \"%s\" is not 1..8 of A-Z 0-9, or is an original car's", card->stem);
        return 0;   /* no files to look for */
    }
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    car_piece_paths(card->stem, paths);
    for (int i = 0; i < CAR_PIECE_FILES; i++) {
        const SetFile *f = find_any_case(files, paths[i]);
        if (f == NULL) {
            report_add(r, "PIECE.FILE_MISSING", 0, "%s is not in the piece", paths[i]);
            continue;
        }
        validate_file(f->data, f->size, paths[i], r);
        if (i == CAR_PIECE_FILES - 1) specs_ok(f, paths[i], r);
    }
    return report_fatal_count(r) == fatal_before;
}
