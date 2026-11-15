/* port (dep59): the rules of a car piece, a mod of type=car (notes/dep59-content-model-design.md), one for the game and the studio. */
#ifndef TNFS_CAR_PIECE_H
#define TNFS_CAR_PIECE_H

enum { CAR_PIECE_FILES = 3, CAR_PIECE_PATH = 64 };

/* "supra" "diablo" "911" "zr1" "512tr" "viper" "nsx" "rx7" -> the race model 0..7; -1 otherwise */
int car_piece_model_of_word(const char *word);
/* the word of a race model 0..7, NULL otherwise */
const char *car_piece_word_of_model(int model);
/* 1 when `stem` may name an added car's files: 1..8 of A-Z 0-9, not a stem of the CD's cars, traffic or police (slots.h, and P911T, the
 * race's ninth), not 'M' + one (the reduced model's), whatever the case */
int car_piece_stem_ok(const char *stem);
/* the three files of the car `stem`, as paths below the piece's root: its CFM, its PDN, its PBS */
void car_piece_paths(const char *stem, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH]);

#endif
