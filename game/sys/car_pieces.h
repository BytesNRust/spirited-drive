/* port (dep59, step 1): the added cars come from the mod folders in effect (mod_set.h). A folder whose mod.ini says type=car is a car
 * piece; car_piece_check (formats/car_piece.h) says whether it may race, and the registry (car_registry.h) gives it its number. Only
 * Restomod has added cars. A reload keeps the number of every car that stays, so a number in another's hand never names another car. */
#ifndef TNFS_CAR_PIECES_H
#define TNFS_CAR_PIECES_H

#include "formats/car_piece.h"
#include "formats/mod_ini.h"

#include <stdint.h>

/* The registry made the pieces of `dirs` (highest first: of two folders with one id the first is taken, the other refused): the cars no
 * longer there taken out, the ones that stay kept with their numbers, the new ones added. `restomod` 0 leaves none. Each piece taken or
 * refused is one log line ("car piece: <id> is car <n>", "car piece: <id> refused: <why>"); a folder of another type, or with no mod.ini,
 * is passed over. How many cars are registered. */
int car_pieces_load(const char *const dirs[], int count, int restomod);

/* car_pieces_load with the mods in effect, when they changed since the last time (mod_set_generation); at a race's start, never during
 * one, and before a car is looked up by id */
void car_pieces_follow(void);

/* port (dep59 step 3): what an added car's gallery reads of its piece (added_gallery.h) */
typedef struct {
    ModInfo info;                                                      /* its mod.ini, read again */
    char dir[CAR_PIECE_DIR_MAX];
    int skins;                                                         /* 1 + the skins/ folders; 0 is the skin as it comes */
    char skin[CAR_PIECE_SKINS_MAX + 1][CAR_PIECE_SKIN_NAME_MAX + 1];   /* skin[0] "", the others in name order */
    uint8_t skin_masked[CAR_PIECE_SKINS_MAX + 1];                      /* 1: the skin has masks (*_mask.png; skin 0: car_masks/<STEM>/) */
} CarPieceCard;

/* the registered added car `car`'s card into `out`: its mod.ini read again from its folder, and its skins: each folder of skins/ named
 * 1..16 of a-z 0-9 '-' that holds a PNG, up to CAR_PIECE_SKINS_MAX (which ones may race is the piece's check's to say). 0 for an
 * original, a number not registered, or a card no longer read. */
int car_pieces_card(int car, CarPieceCard *out);

#endif
