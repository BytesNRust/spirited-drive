/* The install check of a mod's zip (dep59 n4), everything decided before anything is written: the zip read with its limits (zip.h),
 * then every entry's name and kind, then every file's content with the readers the game itself uses. One refusal refuses the whole mod,
 * with the reason for the player. What passes is a list of the entries to install and where: the installer unpacks those and nothing
 * else, from the same bytes that were checked.
 *  - Names: a safe relative path (modpath_clean); an entry that is a link or a special file, two names that are the same but for case, or
 *    a file and a folder of one name, refuse the mod.
 *  - The zip of a folder: when the root has no mod.ini but one folder holds everything and its own mod.ini, that folder is the root.
 *  - What a mod may carry, and nothing else (mod_file_kind): the game's files, in a folder the CD has and with an extension that folder
 *    holds there (SIMDATA/, FRONTEND/: docs/modding/inventory.md; never GAMEDATA/, the player's own), the port's (track_light/ and track/ text, car_masks/
 *    dash_masks/ track_masks/ PNG, sounds/ WAV, car_envmap.png), its card (mod.ini, required), its picture (preview.png, up to
 *    MOD_PREVIEW_W x MOD_PREVIEW_H) and a README or LICENSE (.txt, .md or none). What macOS and Windows add on their own (__MACOSX/,
 *    .DS_Store, ._*, Thumbs.db, desktop.ini) is left out, not refused, and never installed.
 *  - What its kind of piece may carry (mod_file_rule), from its card, read first: a tweak's file that changes the race, a car's file that
 *    is not its own, or a track's file that is not its own, refuse the mod; a car passes car_piece_check as well, a track track_piece_check.
 *  - Content: a game file passes the original's limits (validate_file, as the overlay checks it at load), a PNG's size is read from its
 *    header and kept within its limits before it is unpacked and read (png_read), a WAV reads (wav_read), the text has no NUL and keeps
 *    within its size, and the card parses (mod_ini.h). */
#ifndef TNFS_MOD_CHECK_H
#define TNFS_MOD_CHECK_H

#include "formats/mod_ini.h"
#include "formats/zip.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MOD_TRACK_CARD_W = 208, MOD_TRACK_CARD_H = 127,   /* a track piece's card.png (the track slot of Control Central: added_card.h) */
    MOD_PREVIEW_W = 512,
    MOD_PREVIEW_H = 256,
    MOD_PNG_MAX_SIDE = 1024,           /* any other picture, a side (the largest the port reads are 640 x 480) */
    MOD_PNG_MAX_PIXELS = 32u << 20,    /* every picture of the mod together */
    MOD_TEXT_MAX = 1u << 20,
    MOD_DOC_MAX = 64u << 10
};

typedef enum {
    MOD_KIND_GAME,    /* a file of the CD's tree, validated as the overlay does */
    MOD_KIND_TEXT,    /* track_light/<x>.txt, track/<x>.txt */
    MOD_KIND_PNG,     /* the masks, car_envmap.png, preview.png */
    MOD_KIND_WAV,     /* sounds/<x>.wav */
    MOD_KIND_CARD,    /* mod.ini */
    MOD_KIND_DOC,     /* README, LICENSE */
    MOD_KIND_COUNT,
    MOD_KIND_SKIP = -2,   /* the system's own litter: left out */
    MOD_KIND_NONE = -1    /* nothing the game reads: refuses the mod */
} ModFileKind;

/* The kind of the file at `rel` (a cleaned path from the mod's root) */
ModFileKind mod_file_kind(const char *rel);

/* dep59 step 2: what each kind of piece may carry, one table for the zip check and the folders the game loads */
typedef enum {
    MOD_FILE_OK,
    MOD_FILE_RACE,      /* changes the race: not in a tweak (docs/modding/inventory.md, "Files that change the race") */
    MOD_FILE_FOREIGN    /* not one of this piece's own files */
} ModFileRule;
/* whether a piece of `type` (MOD_TYPE_*) whose stem is `stem` ("" for a tweak) may carry the file at `rel` (a cleaned path from its root,
 * any case). A tweak: anything mod_file_kind takes but what changes the race. A car: its card, preview and README / LICENSE, its three
 * files (car_piece_paths) and car_masks/<STEM>/, dash_masks/<STEM>/. A track: its card, preview and README / LICENSE and the files track_piece_owns names. */
ModFileRule mod_file_rule(int type, const char *stem, const char *rel);

/* 1 when `rel` is a file named by the stem of a piece of `type` (MOD_TYPE_CAR or MOD_TYPE_TRACK) with that `stem`: its three car files, masks and skins, a track's stage files, light, track and mask files (track_piece_owns); never a
 * file the pieces share (mod.ini, README, preview.png, car_envmap.png, sounds). An installed tweak acts on the original game only: what it carries for such a file of another mod's piece is left out. */
int mod_file_of_piece(int type, const char *stem, const char *rel);
/* 1 when the text of a track light file (track_light/<TRACK>.txt) holds a line whose first word is `tunnel`, `snow` or `snow_map`: those facts change the race (the dry zones and the snow's grip), which an installed tweak may not;
 * its light (brightness, tone, lamps, tunnel_lamp, tunnel_tone, tunnel_look, sun, time, rows) is no such fact */
int mod_track_light_changes_race(const char *text, size_t n);

/* One entry to install: the zip entry's index and the path below the mod's folder ('/' between parts, no "." or ".."). */
typedef struct {
    size_t index;
    char path[ZIP_NAME_MAX + 1];
} ModEntry;

typedef struct {
    ModInfo info;
    char root[ZIP_NAME_MAX + 1];   /* "" or the folder that is the mod's root, with its '/' */
    ModEntry *entries;              /* the files to install, in the zip's order (mod_check_free frees them) */
    size_t entry_count;
    uint64_t bytes;                 /* their size unpacked */
    size_t kinds[MOD_KIND_COUNT];
    int has_preview;
} ModCheck;

/* 1 and `out` filled when the zip is a mod the game can install (mod_check_free it); 0 with the reason in `why` otherwise, and `out`
 * holds nothing to free. */
int mod_check_zip(const uint8_t *zip, size_t size, ModCheck *out, char *why, size_t why_cap);
void mod_check_free(ModCheck *c);

#endif
