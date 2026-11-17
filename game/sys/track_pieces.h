/* port (dep59 step 5): the added tracks come from the mod folders in effect (mod_set.h). A folder whose mod.ini says type=track is a track
 * piece; track_piece_check (formats/track_piece.h) says whether it may race, and the registry (track_registry.h) gives it its number. Only
 * Restomod has added tracks. A reload keeps the number of every track that stays, so a number in another's hand never names another track.
 * Same shape as car_pieces.h. */
#ifndef TNFS_TRACK_PIECES_H
#define TNFS_TRACK_PIECES_H

/* The registry made the pieces of `dirs` (highest first: of two folders with one id the first is taken, the other refused): the tracks no
 * longer there taken out, the ones that stay kept with their numbers, the new ones added. `restomod` 0 leaves none. Each piece taken or
 * refused is one log line ("track piece: <id> is track <n>", "track piece: <id> refused: <why>"); a folder of another type, or with no
 * mod.ini, is passed over. How many tracks are registered. */
int track_pieces_load(const char *const dirs[], int count, int restomod);

/* track_pieces_load with the mods in effect, when they changed since the last time (mod_set_generation); at a race's start, never during one,
 * and before a track is looked up by id */
void track_pieces_follow(void);

#endif
