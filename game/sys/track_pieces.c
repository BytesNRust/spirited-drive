/* See track_pieces.h. */
#include "game/sys/track_pieces.h"

#include "formats/exchange.h"
#include "formats/mod_ini.h"
#include "formats/showcase_tint.h"
#include "formats/track_piece.h"
#include "formats/validate.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/sys/track_registry.h"
#include "platform/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { PATH_MAX_LEN = 1200, DIRS_MAX = 64 };

/* `dir`/`rel` read into `set` under `rel` as found: as it is, in upper case or in lower case (the overlay's rule) */
static void take_file(FileSet *set, const char *dir, const char *rel)
{
    char name[TRACK_PIECE_PATH], path[PATH_MAX_LEN];
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

/* the track piece in `dir` into `out`: 1 when it is one that may race; 0 when the folder is no track piece (`card` empty) or a refused one
 * (`card` holds it, the reason in `why`) */
static int read_piece(const char *dir, ModInfo *card, TrackPiece *out, char *why, size_t why_cap)
{
    char path[PATH_MAX_LEN];
    memset(card, 0, sizeof *card);
    snprintf(path, sizeof path, "%s/mod.ini", dir);
    size_t size = 0;
    uint8_t *text = platform_read_file(path, &size);
    if (text == NULL) return 0;
    const int parsed = mod_ini_parse(text, size, card, why, why_cap);
    platform_free_file(text);
    if (!parsed || card->type != MOD_TYPE_TRACK) return memset(card, 0, sizeof *card), 0;   /* not a track piece: mod_check's to say */
    TrackPieceSpec spec;
    Report r;
    report_init(&r);
    track_piece_spec(card, &spec, &r);   /* the stages to read; the check below says what is wrong with a card that is not good */
    report_free(&r);
    FileSet set;
    fileset_init(&set);
    if (track_piece_stem_ok(card->stem)) {
        for (int stage = 1; stage <= spec.segments; stage++) {
            char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH];
            track_piece_paths(card->stem, stage, paths);
            for (int i = 0; i < TRACK_PIECE_FILES; i++) take_file(&set, dir, paths[i]);
        }
    }
    report_init(&r);
    const int ok = track_piece_check(card, &set, &spec, &r);
    if (!ok) first_fatal(&r, why, why_cap);
    report_free(&r);
    const uint32_t print = ok ? track_piece_fingerprint(&spec, card->stem, &set) : 0;
    fileset_free(&set);
    if (!ok) return 0;
    memset(out, 0, sizeof *out);
    out->spec = spec;
    out->fingerprint = print;
    out->card_name_off = card->card_name_off;
    out->tinted = card->showcase_tint[0] != '\0' && showcase_tint_parse(card->showcase_tint, out->tint);   /* (the check refused a bad one) */
    if (!out->tinted) memcpy(out->tint, SHOWCASE_TINT_SEPIA, sizeof out->tint);
    snprintf(out->id, sizeof out->id, "%s", card->id);
    snprintf(out->name, sizeof out->name, "%s", card->name);
    snprintf(out->stem, sizeof out->stem, "%s", card->stem);
    snprintf(out->author, sizeof out->author, "%s", card->author);
    snprintf(out->description, sizeof out->description, "%s", card->description);
    snprintf(out->length, sizeof out->length, "%s", card->length);
    if (snprintf(out->dir, sizeof out->dir, "%s", dir) >= (int)sizeof out->dir)
        return snprintf(why, why_cap, "its folder's path is longer than %d characters", (int)sizeof out->dir - 1), 0;
    return 1;
}

/* 1 when one of the `count` pieces of `taken` has id `id` */
static int has_id(const TrackPiece *taken, int count, const char *id)
{
    for (int i = 0; i < count; i++)
        if (strcmp(taken[i].id, id) == 0) return 1;
    return 0;
}

int track_pieces_load(const char *const dirs[], int count, int restomod)
{
    static TrackPiece taken[TRACK_ADDED_MAX];
    int n = 0;
    char why[300];
    for (int d = 0; restomod && dirs != NULL && d < count; d++) {
        if (dirs[d] == NULL) continue;
        ModInfo card;
        TrackPiece p;
        why[0] = '\0';
        const int ok = read_piece(dirs[d], &card, &p, why, sizeof why);
        if (card.id[0] == '\0') continue;   /* no track piece */
        if (!ok) port_log("track piece: %s refused: %s\n", card.id, why);
        else if (has_id(taken, n, p.id)) port_log("track piece: %s refused: a folder before it has that id\n", p.id);
        else if (n == TRACK_ADDED_MAX) port_log("track piece: %s refused: there are %d added tracks already, the most there may be\n", p.id, TRACK_ADDED_MAX);
        else taken[n++] = p;
    }
    for (int track = TRACK_ADDED_FIRST; track < TRACK_ADDED_FIRST + TRACK_ADDED_MAX; track++) {   /* the ones no longer there, out first: their stems are free */
        const TrackPiece *had = track_registry_piece(track);
        if (had == NULL || has_id(taken, n, had->id)) continue;
        char id[sizeof had->id];
        snprintf(id, sizeof id, "%s", had->id);
        track_registry_remove(id);
    }
    for (int i = 0; i < n; i++) {
        const int track = track_registry_find(taken[i].id) >= 0 ? track_registry_update(&taken[i], why, sizeof why)
                                                                : track_registry_add(&taken[i], why, sizeof why);
        if (track < 0) {
            port_log("track piece: %s refused: %s\n", taken[i].id, why);
            track_registry_remove(taken[i].id);
        } else {
            port_log("track piece: %s is track %d\n", taken[i].id, track);
        }
    }
    return track_registry_count();
}

void track_pieces_follow(void)
{
    static uint32_t loaded;
    const uint32_t now = mod_set_generation();
    if (now == loaded) return;
    loaded = now;
    const char *dirs[DIRS_MAX];
    const int count = mod_set_dirs(dirs, DIRS_MAX);
    track_pieces_load(dirs, count, port_settings_load().profile == PROFILE_RESTOMOD);
}
