/* A mod's card (dep59 n4): mod.ini at the root of its zip and of its folder. Lines `key=value` (spaces round both trimmed, CR LF or LF),
 * blank lines and lines starting with '#'. Keys: id (required; a-z, 0-9 and '-', not first; up to MOD_INI_ID_MAX; the folder's name, so
 * "legacy" is kept for the loose files moved there, and no name Windows keeps for a device: con, prn, aux, nul, com0-9, lpt0-9), name and version (required), author and description; a key the game does not know is
 * left for a later version, whatever it holds, and is never said back; a key twice is refused. Values are UTF-8 text kept as it is, with
 * no control character (C0, DEL, C1) and no bidirectional mark that could change what the player reads.
 * port (dep59): what the piece is (notes/dep59-content-model-design.md). type: tweak (also when not given), car or track. A car's keys:
 * stem (its files' root, 1..8 of A-Z 0-9), short_name (text, up to 8), and sound_from, dash_from (an original car's word, 1..8 of
 * a-z 0-9; which words name a car is the registry's to say); ai_from is still read so that car_piece_check refuses it with its
 * replacement (3b), and ai_level (a word: car_ai.h says which are levels). Whether a car piece has what it needs is the loader's to say, not the card's.
 * port (dep59 step 3): a car's figures for its gallery, year, power and weight (text up to 16, shown as written: "1987", "478 hp"), and
 * paints=Name:rrggbb,Name:rrggbb, its Color choices: up to 16, a name 1..24 of text without ':' or ',', six hex digits in any case, no two
 * names alike in any case.
 * port (dep59, gallery and showcase plan section 1): card_name, on (the default) or off: whether the game draws the car's name over its
 * card.png and rival.png (off for pictures that show their own).
 * port (dep59 step 5): a track's keys, read as written and judged by track_piece_check (notes/dep59-step5-tracks-design.md): stem (as a car's),
 * kind, segments, road and edge (a word, a-z 0-9, up to 8), length (text up to 16: "14.2 km"), laps ("4,8,16") and times ("dawn,day,dusk,night",
 * up to 32).
 * port (dep59 showcase, the owner 2026-11-17): a car's category (text up to 16, shown as written: "B"), photos, the big pictures of its showcase
 * page in order (up to MOD_INI_PHOTOS_MAX file names, a-z 0-9 '-' '_' '.', ending ".png", up to MOD_INI_PHOTO_NAME_MAX, none of the names the game
 * gives its own pictures: card, rival, gallery) and photo_small, the one small picture. The files are the car piece's to judge. */
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
    MOD_INI_FROM_MAX = 8,
    MOD_INI_FIGURE_MAX = 16,
    MOD_INI_PAINTS_MAX = 16,
    MOD_INI_PAINT_NAME_MAX = 24,
    MOD_INI_TIMES_MAX = 32,
    MOD_INI_TINT_MAX = 7,   /* "#RRGGBB" */
    MOD_INI_CATEGORY_MAX = 16,
    MOD_INI_MAKE_MAX = 32,   /* make=, model= and country= (text, up to 32) */
    MOD_INI_PHOTOS_MAX = 8,
    MOD_INI_PHOTO_NAME_MAX = 24
};

typedef struct {
    char name[MOD_INI_PAINT_NAME_MAX + 1];
    uint8_t rgb[3];
} ModPaint;

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
    char assist_abs[MOD_INI_FROM_MAX + 1], assist_traction[MOD_INI_FROM_MAX + 1];   /* port (dep78): abs= and traction=, yes or no as written (car_piece_assist); "" when not given: the PBS says */
    char ai_level[MOD_INI_FROM_MAX + 1];   /* port (dep59 3b): slow, medium, fast or top (car_ai.h); "": its own pace */
    /* the author's figures, as written; "" when not given */
    char year[MOD_INI_FIGURE_MAX + 1], power[MOD_INI_FIGURE_MAX + 1], weight[MOD_INI_FIGURE_MAX + 1];
    ModPaint paints[MOD_INI_PAINTS_MAX];   /* paints=, in its order */
    int paint_count;
    /* a track's keys as written; "" when not given */
    char kind[MOD_INI_FROM_MAX + 1], segments[MOD_INI_FROM_MAX + 1], road[MOD_INI_FROM_MAX + 1], edge[MOD_INI_FROM_MAX + 1];
    char length[MOD_INI_FIGURE_MAX + 1], laps[MOD_INI_FIGURE_MAX + 1], times[MOD_INI_TIMES_MAX + 1];
    char category[MOD_INI_CATEGORY_MAX + 1];   /* "" when not given */
    char make[MOD_INI_MAKE_MAX + 1], model[MOD_INI_MAKE_MAX + 1], country[MOD_INI_MAKE_MAX + 1];   /* a car's make, model and country as written (owner 2026-11-18: kept, not shown yet); "" when not given */
    char showcase_tint[MOD_INI_TINT_MAX + 1];   /* a track's showcase_tint=#RRGGBB as written (showcase_tint.h); "" when not given */
    char photo[MOD_INI_PHOTOS_MAX][MOD_INI_PHOTO_NAME_MAX + 1];   /* photos=, in order */
    int photo_count;
    char photo_small[MOD_INI_PHOTO_NAME_MAX + 1];   /* "" when not given */
    int card_name_off;   /* card_name=off: the game draws no name over card.png and rival.png (the picture shows its own); on by default */
} ModInfo;

/* 1 and `out` filled when the card is good; 0 with the reason in `why` (English, for the player) otherwise. */
int mod_ini_parse(const uint8_t *text, size_t size, ModInfo *out, char *why, size_t why_cap);

/* port (dep59 showcase): 1 when `name` is a photo's file name (the rule above: photos=, photo_small=) */
int mod_ini_photo_name_ok(const char *name);

/* port (newcomer pains): the lines of a card `text` whose keys a generator does not write (every key but id, name, version, type, stem,
 * kind, segments, road, edge, author and description), each as "key=value\n" into `out` (`cap` bytes, NUL terminated), comments and blank
 * lines left out. The generated keys of a rewritten card come from the generator; these come from the hand-written one. 0 when they do not fit. */
int mod_ini_others(const uint8_t *text, size_t size, char *out, size_t cap, size_t *len);

/* port (newcomer pains): the author and description lines of a card: "author=TEXT" and "description=TEXT" when given, else a comment
 * "# author=" that shows the key (the game refuses an empty value). 1 when they fit in `out` (`cap` bytes, NUL terminated). */
int mod_ini_text_keys(const char *author, const char *description, char *out, size_t cap);

/* 1 when `id` is a usable mod id (the rule above) */
int mod_ini_id_ok(const char *id);

#endif
