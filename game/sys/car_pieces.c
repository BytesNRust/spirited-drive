/* See car_pieces.h. */
#include "game/sys/car_pieces.h"

#include "formats/car_ai.h"
#include "formats/car_piece.h"
#include "formats/crc32.h"
#include "game/sys/piece_print.h"
#include "formats/mod_ini.h"
#include "formats/pdn.h"
#include "formats/validate.h"
#include "game/race/car_registry.h"
#include "game/sys/data_path.h"
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

/* port (dep59 step 3): the file `rel` of `dir` into `set` by that name alone (a piece's pictures: the game reads them so). One that is
 * not a regular file or is larger than a picture may be goes in empty, so the check refuses it as no PNG; one not there stays out */
static void take_exact(FileSet *set, const char *dir, const char *rel)
{
    char path[PATH_MAX_LEN];
    if (snprintf(path, sizeof path, "%s/%s", dir, rel) >= (int)sizeof path) return;
    size_t size = 0;
    const int there = data_path_file_size(path, &size);
    if (there == 0) return;
    if (there < 0 || size > CAR_PIECE_PICTURE_BYTES_MAX) {
        port_log("car piece: %s is not a picture's file of up to %d MB\n", path, CAR_PIECE_PICTURE_BYTES_MAX >> 20);
        fileset_add(set, rel, (const uint8_t *)"", 0);
        return;
    }
    uint8_t *data = platform_read_file(path, &size);
    if (data == NULL) return;
    fileset_add(set, rel, data, size);
    platform_free_file(data);
}

/* port (dep59 step 3): the piece's pictures and the PNGs of each folder of its skins/, for car_piece_check; one folder past the most it
 * may have is read, so the check says there are too many */
static void take_pictures(FileSet *set, const char *dir, const ModInfo *card)
{
    static const char *const PICTURES[] = { "card.png", "rival.png", "gallery.png" };
    for (size_t i = 0; i < sizeof PICTURES / sizeof PICTURES[0]; i++) take_exact(set, dir, PICTURES[i]);
    for (int i = 0; i < card->photo_count; i++) take_exact(set, dir, card->photo[i]);   /* port (dep59 showcase): photos=, photo_small= */
    if (card->photo_small[0] != '\0') take_exact(set, dir, card->photo_small);
    char path[PATH_MAX_LEN];
    if (snprintf(path, sizeof path, "%s/skins", dir) >= (int)sizeof path) return;
    int count = 0, taken = 0;
    char **names = platform_list_files(path, "*", &count);
    for (int i = 0; i < count && taken <= CAR_PIECE_SKINS_MAX; i++) {
        char folder[PATH_MAX_LEN];
        if (snprintf(folder, sizeof folder, "%s/skins/%s", dir, names[i]) >= (int)sizeof folder) continue;
        int files = 0;
        char **pngs = platform_list_files(folder, "*.png", &files);
        for (int k = 0; k < files && k <= CAR_PIECE_SKIN_FILES_MAX; k++) {   /* one past the most: the check says so */
            char rel[CAR_PIECE_PATH];
            if (snprintf(rel, sizeof rel, "skins/%s/%s", names[i], pngs[k]) < (int)sizeof rel) take_exact(set, dir, rel);
        }
        if (pngs != NULL) platform_free_names(pngs);
        taken += files > 0;
    }
    if (names != NULL) platform_free_names(names);
}

/* the file of `set` at `path` (any case), NULL when there is none */
static const SetFile *file_at(const FileSet *set, const char *path)
{
    for (size_t i = 0; i < set->count; i++)
        if (strcasecmp(set->file[i].name, path) == 0) return &set->file[i];
    return NULL;
}

/* port (dep59 1b): the CRC-32 of the PBS then the PDN in `set` (their paths in `paths`), then (3b) `ai`'s pace and coast, 4 bytes LE each:
 * what decides how the car drives; 1 for a 0 */
static uint32_t fingerprint(const FileSet *set, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], const CarAiTuning *ai)
{
    uint32_t crc = 0;
    static const int ORDER[2] = { 2, 1 };   /* car_piece_paths': the PBS, then the PDN */
    for (int k = 0; k < 2; k++) {
        const SetFile *f = file_at(set, paths[ORDER[k]]);
        if (f != NULL) crc = crc32_update(crc, f->data, f->size);
    }
    uint8_t tail[8];
    for (int i = 0; i < 4; i++) tail[i] = (uint8_t)((uint32_t)ai->pace >> (8 * i)), tail[4 + i] = (uint8_t)((uint32_t)ai->coast >> (8 * i));
    crc = crc32_update(crc, tail, sizeof tail);
    return crc != 0 ? crc : 1;
}

/* port (dep59 step 5, records by id + print): the same stream as `fingerprint` (the PBS, the PDN, the AI's pace and coast) under the SHA-256 */
static void print_of(const FileSet *set, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], const CarAiTuning *ai, uint8_t out[PIECE_PRINT_SIZE])
{
    PiecePrint pp;
    piece_print_begin(&pp, PIECE_PRINT_CAR);
    static const int ORDER[2] = { 2, 1 };
    for (int k = 0; k < 2; k++) {
        const SetFile *f = file_at(set, paths[ORDER[k]]);
        if (f != NULL) piece_print_add(&pp, f->data, f->size);
    }
    uint8_t tail[8];
    for (int i = 0; i < 4; i++) tail[i] = (uint8_t)((uint32_t)ai->pace >> (8 * i)), tail[4 + i] = (uint8_t)((uint32_t)ai->coast >> (8 * i));
    piece_print_add(&pp, tail, sizeof tail);
    piece_print_end(&pp, out);
}

/* port (dep59 3b): the AI tuning of the piece: from its PDN (`paths`[1] in `set`), or its card's ai_level */
static int piece_ai(const FileSet *set, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], const char *level, CarAiTuning *out, char *why,
                    size_t why_cap)
{
    const SetFile *f = file_at(set, paths[1]);
    uint8_t pdn[PDN_SIZE];
    if (f == NULL || !pdn_read(pdn, f->data, f->size)) return snprintf(why, why_cap, "its PDN cannot be read"), 0;
    return car_ai_tuning(pdn, level, out, why, why_cap);
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
    take_pictures(&set, dir, card);
    Report r;
    report_init(&r);
    int ok = car_piece_check(card, &set, &r);
    if (!ok) first_fatal(&r, why, why_cap);
    report_free(&r);
    CarAiTuning ai = { 0, 0 };
    ok = ok && piece_ai(&set, paths, card->ai_level, &ai, why, why_cap);
    const uint32_t print = ok ? fingerprint(&set, paths, &ai) : 0;
    fileset_free(&set);
    if (!ok) return 0;
    memset(out, 0, sizeof *out);
    out->fingerprint = print;
    if (ok) print_of(&set, paths, &ai, out->print);
    out->ai = ai;
    snprintf(out->id, sizeof out->id, "%s", card->id);
    snprintf(out->name, sizeof out->name, "%s", card->name);
    snprintf(out->stem, sizeof out->stem, "%s", card->stem);
    snprintf(out->short_name, sizeof out->short_name, "%s", card->short_name[0] != '\0' ? card->short_name : card->stem);
    out->sound_model = car_piece_model_of_word(card->sound_from);
    out->dash_model = card->dash_from[0] != '\0' ? car_piece_model_of_word(card->dash_from) : -1;
    out->card_name_off = card->card_name_off;
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

/* how many files of `dir` match `pattern` */
static int files_in(const char *dir, const char *pattern)
{
    int count = 0;
    char **names = platform_list_files(dir, pattern, &count);
    if (names != NULL) platform_free_names(names);
    return count;
}

int car_pieces_card(int car, CarPieceCard *out)
{
    memset(out, 0, sizeof *out);
    const CarPiece *p = car_registry_piece(car);
    if (p == NULL || p->dir[0] == '\0') return 0;
    char path[PATH_MAX_LEN], why[160];
    snprintf(path, sizeof path, "%s/mod.ini", p->dir);
    size_t size = 0;
    uint8_t *text = platform_read_file(path, &size);
    const int parsed = text != NULL && mod_ini_parse(text, size, &out->info, why, sizeof why);
    if (text != NULL) platform_free_file(text);
    if (!parsed) return port_log("car piece: %s: its mod.ini is no longer read\n", p->id), memset(out, 0, sizeof *out), 0;
    if (strcmp(out->info.id, p->id) != 0)   /* changed since the registry read it: its looks would be kept under another id */
        return port_log("car piece: %s: its mod.ini now names %s\n", p->id, out->info.id), memset(out, 0, sizeof *out), 0;
    snprintf(out->dir, sizeof out->dir, "%s", p->dir);
    out->skins = 1;
    snprintf(path, sizeof path, "%s/car_masks/%s", p->dir, p->stem);
    out->skin_masked[0] = files_in(path, "*_mask.png") > 0;
    int count = 0;
    snprintf(path, sizeof path, "%s/skins", p->dir);
    char **names = platform_list_files(path, "*", &count);
    for (int i = 0; i < count && out->skins <= CAR_PIECE_SKINS_MAX; i++) {
        char folder[PATH_MAX_LEN];
        if (!car_piece_skin_name_ok(names[i], strlen(names[i])) || snprintf(folder, sizeof folder, "%s/skins/%s", p->dir, names[i]) >= (int)sizeof folder ||
            files_in(folder, "*.png") == 0)
            continue;
        snprintf(out->skin[out->skins], sizeof out->skin[0], "%s", names[i]);
        out->skin_masked[out->skins++] = files_in(folder, "*_mask.png") > 0;
    }
    if (names != NULL) platform_free_names(names);
    return 1;
}
