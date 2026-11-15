/* The install check of a mod's zip (dep59 n4), everything decided before anything is written: the zip read with its limits (zip.h),
 * then every entry's name and kind, then every file's content with the readers the game itself uses. One refusal refuses the whole mod,
 * with the reason for the player.
 *  - Names: a safe relative path (modpath_clean); a symbolic link, or two names that are the same but for case, refuse the mod.
 *  - The zip of a folder: when the root has no mod.ini but one folder holds everything and its own mod.ini, that folder is the root.
 *  - What a mod may carry, and nothing else (mod_file_kind): the game's files (SIMDATA/, GAMEDATA/, FRONTEND/), the port's (track_light/
 *    and track/ text, car_masks/ dash_masks/ track_masks/ PNG, sounds/ WAV, car_envmap.png), its card (mod.ini, required), its picture
 *    (preview.png, up to MOD_PREVIEW_W x MOD_PREVIEW_H) and a README or LICENSE (.txt, .md or none). What macOS and Windows add on their own
 *    (__MACOSX/, .DS_Store, Thumbs.db, desktop.ini) is left out, not refused.
 *  - Content: a game file passes the original's limits (validate_file, as the overlay checks it at load), a PNG reads (png_read), a WAV
 *    reads (wav_read), the text has no NUL and keeps within its size, and the card parses (mod_ini.h). */
#ifndef TNFS_MOD_CHECK_H
#define TNFS_MOD_CHECK_H

#include "formats/mod_ini.h"
#include "formats/zip.h"

#include <stddef.h>
#include <stdint.h>

enum { MOD_PREVIEW_W = 512, MOD_PREVIEW_H = 256, MOD_TEXT_MAX = 1u << 20, MOD_DOC_MAX = 64u << 10 };

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

typedef struct {
    ModInfo info;
    char root[ZIP_NAME_MAX + 1];   /* "" or the folder that is the mod's root, with its '/' */
    size_t files;                   /* the files to install (the litter left out) */
    uint64_t bytes;                 /* their size unpacked */
    size_t kinds[MOD_KIND_COUNT];
    int has_preview;
} ModCheck;

/* 1 and `out` filled when the zip is a mod the game can install; 0 with the reason in `why` otherwise. */
int mod_check_zip(const uint8_t *zip, size_t size, ModCheck *out, char *why, size_t why_cap);

/* The path an entry is installed at, below the mod's folder: its cleaned name without `root`. 0 when it has none (unsafe, or not under
 * the root). The installer calls it for each entry mod_check_zip passed. */
int mod_check_install_path(const ModCheck *c, const char *entry_name, char *rel, size_t cap);

#endif
