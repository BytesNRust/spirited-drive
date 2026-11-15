/* port (dep59, step 1): the added cars come from the mod folders in effect (mod_set.h). A folder whose mod.ini says type=car is a car
 * piece; car_piece_check (formats/car_piece.h) says whether it may race, and the registry (car_registry.h) gives it its number. Only
 * Restomod has added cars. A reload keeps the number of every car that stays, so a number in another's hand never names another car. */
#ifndef TNFS_CAR_PIECES_H
#define TNFS_CAR_PIECES_H

/* The registry made the pieces of `dirs` (highest first: of two folders with one id the first is taken, the other refused): the cars no
 * longer there taken out, the ones that stay kept with their numbers, the new ones added. `restomod` 0 leaves none. Each piece taken or
 * refused is one log line ("car piece: <id> is car <n>", "car piece: <id> refused: <why>"); a folder of another type, or with no mod.ini,
 * is passed over. How many cars are registered. */
int car_pieces_load(const char *const dirs[], int count, int restomod);

/* car_pieces_load with the mods in effect, when they changed since the last time (mod_set_generation); at a race's start, never during
 * one, and before a car is looked up by id */
void car_pieces_follow(void);

#endif
