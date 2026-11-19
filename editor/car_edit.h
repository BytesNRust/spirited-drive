/* The edits of a car's card (dep59 car editor 1e): the fields and the colors of mod.ini as the author has them on screen, with undo and redo, the problems each
 * field has by the game's own rules (mod_ini_parse, car_piece's words), and Save, which writes only mod.ini and only the lines that changed (mod_ini_set: every
 * other byte stays). Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_CAR_EDIT_H
#define TNFS_TRACK_EDITOR_CAR_EDIT_H

#include "formats/mod_ini.h"

#include <stddef.h>
#include <stdint.h>

/* the card's fields the author edits; the text of each is "" when the card does not give the key */
enum {
    CE_VERSION, CE_YEAR, CE_POWER, CE_WEIGHT, CE_MAKE, CE_MODEL, CE_COUNTRY, CE_CATEGORY, CE_AUTHOR, CE_DESCRIPTION,
    CE_ABS,           /* "yes", "no" or "" (the car's own file says) */
    CE_TRACTION,      /* the same */
    CE_SOUND_FROM,    /* the word of an original car: car_piece_word_of_model */
    CE_FIELDS,
    CE_PAINTS = CE_FIELDS,   /* the colors, not a text: a problem about them is for this field */
    CE_CARD = CE_FIELDS + 1  /* the card as a whole (its size) */
};

enum {
    CE_VALUE_MAX = 512,       /* room to type past the longest field, so that a too long text can be shown as a problem */
    CE_UNDO_MAX = 64,
    CE_PROBLEM_TEXT = 256,
    CE_PROBLEMS_MAX = CE_FIELDS + 2
};

typedef struct {
    char value[CE_FIELDS][CE_VALUE_MAX];
    ModPaint paints[MOD_INI_PAINTS_MAX + 1];   /* one more than the card holds: a 17th is a problem, not a crash */
    int paint_count;
} CarEditState;

typedef struct {
    char path[4096];           /* the card's file */
    char original[MOD_INI_MAX_SIZE + 1];   /* its bytes as they are now on disk (as of the last Save) */
    size_t original_len;
    CarEditState now, saved;   /* on screen, and as the card has it */
    CarEditState *undo, *redo; /* malloc'd stacks of CE_UNDO_MAX; the oldest falls off */
    int undo_count, redo_count;
    int run;                   /* what the last edit was, so that typing in one field is one step to undo; -1 for none */
} CarEdit;

typedef struct {
    int field;                 /* CE_*; for CE_PAINTS the color is `paint` (-1 when it is the list) */
    int paint;
    char text[CE_PROBLEM_TEXT];   /* English: what is wrong with the number, and what it costs (Save is blocked) */
} CarEditProblem;

const char *car_edit_key(int field);          /* the card's key; "paints" for CE_PAINTS */
const char *car_edit_label(int field);        /* what the window calls it */
size_t car_edit_cap(int field);               /* the longest value the card holds, in bytes; 0 for the words and the colors */

/* Reads the card at `path` (it must parse as a car's card). 1, or 0 with `why`. */
int car_edit_open(CarEdit *e, const char *path, char *why, size_t why_cap);
void car_edit_free(CarEdit *e);

/* Edits; each is one step to undo (typing in one field, or dragging one color, is one step). 1 when something changed, 0 when it was the same or refused
 * (a 17th color; a color that is not there). */
int car_edit_set(CarEdit *e, int field, const char *value);
int car_edit_paint_add(CarEdit *e, const char *name, const uint8_t rgb[3]);
int car_edit_paint_remove(CarEdit *e, int index);
int car_edit_paint_move(CarEdit *e, int index, int delta);   /* delta -1 up, +1 down */
int car_edit_paint_name(CarEdit *e, int index, const char *name);
int car_edit_paint_rgb(CarEdit *e, int index, const uint8_t rgb[3]);

int car_edit_undo(CarEdit *e);
int car_edit_redo(CarEdit *e);
int car_edit_can_undo(const CarEdit *e);
int car_edit_can_redo(const CarEdit *e);

/* 1 when what is on screen is not what the card has (undoing back to the card's state is not modified). Spaces round a text do not count: Save drops them. */
int car_edit_modified(const CarEdit *e);

/* The problems of what is on screen, by the game's rules, at most `cap` (CE_PROBLEMS_MAX is enough): each names the field, says in English what is wrong with the number
 * and that Save is blocked. Returns how many. 0 means Save may go. */
int car_edit_problems(const CarEdit *e, CarEditProblem *out, int cap);

/* The card as Save would write it: the original bytes with only the changed lines changed. 1 with `out` (`cap` bytes, `*len` its length); 0 with `why` when a change cannot be
 * written (the card would be over MOD_INI_MAX_SIZE, a value mod_ini_set refuses). */
int car_edit_render(const CarEdit *e, char *out, size_t cap, size_t *len, char *why, size_t why_cap);

/* Writes mod.ini (through a temporary file, then renamed) when nothing has a problem and something changed; nothing else on disk is touched. 1 when it was written or
 * there was nothing to write; 0 with `why` (the first problem, or the write's) and the file as it was. */
int car_edit_save(CarEdit *e, char *why, size_t why_cap);

/* The edit of `key` (as "year=1984") for a headless run: sets the field, a color list is not reachable this way. 1, or 0 when the key is not an editable one. */
int car_edit_set_by_text(CarEdit *e, const char *key_value);

#endif
