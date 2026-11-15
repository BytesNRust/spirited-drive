/* A mod's card (dep59 n4): mod.ini at the root of its zip and of its folder. Lines `key=value` (spaces round both trimmed, CR LF or LF),
 * blank lines and lines starting with '#'. Keys: id (required; a-z, 0-9 and '-', not first; up to MOD_INI_ID_MAX; the folder's name, so
 * "legacy" is kept for the loose files moved there), name and version (required), author and description; a key the game does not know is
 * left for a later version, whatever it holds, and is never said back; a key twice is refused. Values are UTF-8 text kept as it is, with
 * no control character (C0, DEL, C1) and no bidirectional mark that could change what the player reads.
 * port (dep59): what the piece is (notes/dep59-content-model-design.md). type: tweak (also when not given), car or track. A car's keys:
 * stem (its files' root, 1..8 of A-Z 0-9), short_name (text, up to 8), and sound_from, ai_from, dash_from (an original car's word, 1..8 of
 * a-z 0-9; which words name a car is the registry's to say). Whether a car piece has what it needs is the loader's to say, not the card's. */
#ifndef TNFS_MOD_INI_H
#define TNFS_MOD_INI_H

#include <stddef.h>
#include <stdint.h>

enum {
    MOD_INI_MAX_SIZE = 4096,
    MOD_INI_ID_MAX = 32,
    MOD_INI_NAME_MAX = 48,
    MOD_INI_VERSION_MAX = 16,
    MOD_INI_AUTHOR_MAX = 48,
    MOD_INI_DESCRIPTION_MAX = 400,
    MOD_INI_STEM_MAX = 8,
    MOD_INI_SHORT_MAX = 8,
    MOD_INI_FROM_MAX = 8
};

enum { MOD_TYPE_TWEAK = 0, MOD_TYPE_CAR = 1, MOD_TYPE_TRACK = 2 };

typedef struct {
    char id[MOD_INI_ID_MAX + 1];
    char name[MOD_INI_NAME_MAX + 1];
    char version[MOD_INI_VERSION_MAX + 1];
    char author[MOD_INI_AUTHOR_MAX + 1];
    char description[MOD_INI_DESCRIPTION_MAX + 1];
    int type;   /* MOD_TYPE_* */
    char stem[MOD_INI_STEM_MAX + 1];   /* "" when not given, as the ones below */
    char short_name[MOD_INI_SHORT_MAX + 1];
    char sound_from[MOD_INI_FROM_MAX + 1], ai_from[MOD_INI_FROM_MAX + 1], dash_from[MOD_INI_FROM_MAX + 1];
} ModInfo;

/* 1 and `out` filled when the card is good; 0 with the reason in `why` (English, for the player) otherwise. */
int mod_ini_parse(const uint8_t *text, size_t size, ModInfo *out, char *why, size_t why_cap);

/* 1 when `id` is a usable mod id (the rule above) */
int mod_ini_id_ok(const char *id);

#endif
