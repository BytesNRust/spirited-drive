/* See car_pieces.h. */
#include "game/sys/car_pieces.h"

#include "formats/car_piece.h"
#include "formats/crc32.h"
#include "formats/mod_ini.h"
#include "formats/validate.h"
#include "game/race/car_registry.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "platform/platform.h"

#include <ctype.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>

enum { PATH_MAX_LEN = 1200, DIRS_MAX = 64 };

/* `dir`/`rel` read into `set` under `rel` as found: as it is, in upper case or in lower case (the overlay's rule) */
static void take_file(FileSet *set, const char *dir, const char *rel)
{
    char name[CAR_PIECE_PATH], path[PATH_MAX_LEN];
    for (int c = 0; c < 3; c++) {
        size_t i = 0;
        for (; rel[i] != '\0' && i + 1 < sizeof name; i++)
            name[i] = (char)(c == 0 ? rel[i] : c == 1 ? toupper((unsigned char)rel[i]) : tolower((unsigned char)rel[i]));
        name[i] = '\0';
        snprintf(path, sizeof path, "%s/%s", dir, name);
        size_t size = 0;
        uint8_t *data = platform_read_file(path, &size);
        if (data == NULL) continue;
        const int ok = fileset_add(set, name, data, size);
        platform_free_file(data);
        if (ok) return;
    }
}

/* port (dep59 1b): the CRC-32 of the PBS then the PDN in `set` (their paths in `paths`, any case), 1 for a 0 */
static uint32_t fingerprint(const FileSet *set, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH])
{
    uint32_t crc = 0;
    static const int ORDER[2] = { 2, 1 };   /* car_piece_paths': the PBS, then the PDN */
    for (int k = 0; k < 2; k++)
        for (size_t i = 0; i < set->count; i++)
            if (strcasecmp(set->file[i].name, paths[ORDER[k]]) == 0) crc = crc32_update(crc, set->file[i].data, set->file[i].size);
    return crc != 0 ? crc : 1;
}

/* the first fatal finding of `r`, as the log says it */
static void first_fatal(const Report *r, char *out, size_t cap)
{
    snprintf(out, cap, "the piece is not good");
    for (size_t i = 0; i < r->count; i++)
        if (r->item[i].limit->fatal) {
            snprintf(out, cap, "%s: %s", r->item[i].limit->id, r->item[i].item);
            return;
        }
}

/* the car piece in `dir` into `out`: 1 when it is one that may race; 0 when the folder is no car piece (`card` empty) or a refused one
 * (`card` holds it, the reason in `why`) */
static int read_piece(const char *dir, ModInfo *card, CarPiece *out, char *why, size_t why_cap)
{
    char path[PATH_MAX_LEN];
    memset(card, 0, sizeof *card);
    snprintf(path, sizeof path, "%s/mod.ini", dir);
    size_t size = 0;
    uint8_t *text = platform_read_file(path, &size);
    if (text == NULL) return 0;
    const int parsed = mod_ini_parse(text, size, card, why, why_cap);
    platform_free_file(text);
    if (!parsed || card->type != MOD_TYPE_CAR) return memset(card, 0, sizeof *card), 0;   /* not a car piece: mod_check's to say */
    FileSet set;
    fileset_init(&set);
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    if (car_piece_stem_ok(card->stem)) {
        car_piece_paths(card->stem, paths);
        for (int i = 0; i < CAR_PIECE_FILES; i++) take_file(&set, dir, paths[i]);
    }
    Report r;
    report_init(&r);
    const int ok = car_piece_check(card, &set, &r);
    if (!ok) first_fatal(&r, why, why_cap);
    report_free(&r);
    const uint32_t print = ok ? fingerprint(&set, paths) : 0;
    fileset_free(&set);
    if (!ok) return 0;
    memset(out, 0, sizeof *out);
    out->fingerprint = print;
    snprintf(out->id, sizeof out->id, "%s", card->id);
    snprintf(out->name, sizeof out->name, "%s", card->name);
    snprintf(out->stem, sizeof out->stem, "%s", card->stem);
    snprintf(out->short_name, sizeof out->short_name, "%s", card->short_name[0] != '\0' ? card->short_name : card->stem);
    out->sound_model = car_piece_model_of_word(card->sound_from);
    out->ai_model = car_piece_model_of_word(card->ai_from);
    out->dash_model = card->dash_from[0] != '\0' ? car_piece_model_of_word(card->dash_from) : -1;
    if (snprintf(out->dir, sizeof out->dir, "%s", dir) >= (int)sizeof out->dir)
        return snprintf(why, why_cap, "its folder's path is longer than %d characters", CAR_PIECE_DIR_MAX - 1), 0;
    return 1;
}

/* 1 when one of the `count` pieces of `taken` has id `id` */
static int has_id(const CarPiece *taken, int count, const char *id)
{
    for (int i = 0; i < count; i++)
        if (strcmp(taken[i].id, id) == 0) return 1;
    return 0;
}

int car_pieces_load(const char *const dirs[], int count, int restomod)
{
    static CarPiece taken[CAR_ADDED_MAX];
    int n = 0;
    char why[300];
    for (int d = 0; restomod && dirs != NULL && d < count; d++) {
        if (dirs[d] == NULL) continue;
        ModInfo card;
        CarPiece p;
        why[0] = '\0';
        const int ok = read_piece(dirs[d], &card, &p, why, sizeof why);
        if (card.id[0] == '\0') continue;   /* no car piece */
        if (!ok) port_log("car piece: %s refused: %s\n", card.id, why);
        else if (has_id(taken, n, p.id)) port_log("car piece: %s refused: a folder before it has that id\n", p.id);
        else if (n == CAR_ADDED_MAX) port_log("car piece: %s refused: there are %d added cars already, the most there may be\n", p.id, CAR_ADDED_MAX);
        else taken[n++] = p;
    }
    for (int car = CAR_ADDED_FIRST; car < CAR_ADDED_FIRST + CAR_ADDED_MAX; car++) {   /* the ones no longer there, out first: their stems are free */
        const CarPiece *had = car_registry_piece(car);
        if (had == NULL || has_id(taken, n, had->id)) continue;
        char id[sizeof had->id];
        snprintf(id, sizeof id, "%s", had->id);
        car_registry_remove(id);
    }
    for (int i = 0; i < n; i++) {
        const int car = car_registry_find(taken[i].id) >= 0 ? car_registry_update(&taken[i], why, sizeof why) : car_registry_add(&taken[i], why, sizeof why);
        if (car < 0) {
            port_log("car piece: %s refused: %s\n", taken[i].id, why);
            car_registry_remove(taken[i].id);
        } else {
            port_log("car piece: %s is car %d\n", taken[i].id, car);
        }
    }
    return car_registry_count();
}

void car_pieces_follow(void)
{
    static uint32_t loaded;
    const uint32_t now = mod_set_generation();
    if (now == loaded) return;
    loaded = now;
    const char *dirs[DIRS_MAX];
    const int count = mod_set_dirs(dirs, DIRS_MAX);
    car_pieces_load(dirs, count, port_settings_load().profile == PROFILE_RESTOMOD);
}
