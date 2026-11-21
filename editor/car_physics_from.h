/* Physics from (dep102): the handling of one of the game's cars copied into a kit, beside Sound from. It is a rewrite of the kit's specs/specs.tsv and specs/pdn.tsv, row by
 * row by name, from the car's PBS and PDN in the game's files; nothing in the game or in mod.ini changes (the key stays the card's own), so the replay fingerprint of the car
 * changes as it does whenever those two files are edited. What is the kit's own stays: the PBS's half_length and half_width (the model's size), its ABS and traction words
 * (the card says them) and the checksum (Build seals it); of the PDN only the mass, the AI speeds and the factors are copied, the rest of it (the collision body, which
 * must match the model) stays. Needs tnfs_tools. */
#ifndef TNFS_TRACK_EDITOR_CAR_PHYSICS_FROM_H
#define TNFS_TRACK_EDITOR_CAR_PHYSICS_FROM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { CAR_PHYSICS_WORDS = 9 };

/* The words Physics from offers: the eight cars you drive by their words (viper ...) and "warrior". Fills `out` (up to `cap`), returns how many. */
int car_physics_words(const char **out, int cap);

/* The stem of the game's car a word names: the eight cars' own, "warrior" is TRAFFC (the Warrior has no stem of its own: its physics are the traffic car's); NULL otherwise. */
const char *car_physics_stem(const char *word);

/* Copies the physics of `word`'s car from the game's files `cd` (SIMDATA/ under it) into `kit`'s specs/specs.tsv and specs/pdn.tsv. 1 when written, with the rows that changed in
 * *specs_rows and *pdn_rows; 0 with `why` in words and the kit as it was. A kit with no specs.tsv or pdn.tsv (it keeps the game's file) is refused. */
int car_physics_from(const char *cd, const char *kit, const char *word, int *specs_rows, int *pdn_rows, char *why, size_t why_cap);

#ifdef __cplusplus
}
#endif

#endif
