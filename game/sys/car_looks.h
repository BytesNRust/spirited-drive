/* port (dep59 step 3): what the player picked among the added cars, kept in Restomod's file (restomod.cfg) by id, never by number (a
 * number is the run's: car_registry.h): car=<id> the player's car, opponent=<id> the head to head rival, look.<id>=<skin>,<paint> the
 * pair the car's gallery picked, each by name ("-" the skin as it comes / the car's own colour), so a piece whose author reorders its
 * skins or paints= keeps the player's pick. Read from the file at every call (nothing kept). In Stock and Restored nothing is read or
 * written: config.dat keeps their cars, which are the originals only. A line of an id that is not loaded stays in the file. */
#ifndef TNFS_CAR_LOOKS_H
#define TNFS_CAR_LOOKS_H

#include "formats/car_piece.h"
#include "formats/mod_ini.h"

#include <stdbool.h>

enum { CAR_LOOKS_SKIN_NAME_MAX = CAR_PIECE_SKIN_NAME_MAX, CAR_LOOKS_NOT_LOADED = -2 };   /* a skin's folder name: 1..16 of a-z 0-9 '-' (the piece's check says which there are) */

typedef struct {
    char skin[CAR_LOOKS_SKIN_NAME_MAX + 1];    /* "" the skin as it comes */
    char paint[MOD_INI_PAINT_NAME_MAX + 1];    /* "" the car's own colour */
} CarLook;

/* car=<id> (opponent=<id> when `opponent`): the number of that id when it is registered; CAR_LOOKS_NOT_LOADED when the line names an id
 * that is not (its mod gone: Control Central opens with car 0); -1 when there is no line, or the mode is not Restomod */
int car_looks_saved_car(int opponent);
/* `car` kept as car= / opponent=: an added car's id written, an original's line taken out (config.dat keeps it). True when the file is
 * as asked; false in Stock and Restored, which write nothing */
bool car_looks_save_car(int opponent, int car);
/* port (dep59 Lunar L2): track=lunar, the Lunar pick (track_pick.h) over config.dat's Rusty rally: 1 with that line, else 0 (none, another
 * value, or out of Restomod). An older binary's loader skips the key it does not know, so it races config.dat's Rusty rally */
int car_looks_saved_lunar(void);
/* track=lunar written (1) or taken out (0); false out of Restomod */
bool car_looks_save_lunar(int lunar);
/* port (dep59 step 5, T7b): the same one key for an added track, track=<id>: the number of that id when it is registered (16..79);
 * CAR_LOOKS_NOT_LOADED when the line names an id that is not (a mod taken away); -1 for no line, and for track=lunar (car_looks_saved_lunar's) */
int car_looks_saved_track(void);
/* track=<id> of the added track `track` written, or the line taken out for a number that is no added track; false out of Restomod. The line is
 * the one key track=: it replaces track=lunar, and track=lunar replaces it */
bool car_looks_save_track(int track);
/* look.<id>=; both "" when there is none or the mode is not Restomod */
CarLook car_looks_get(const char *id);
/* look.<id>= written (the line taken out when both are ""); false for a bad id or name, or out of Restomod */
bool car_looks_set(const char *id, const CarLook *look);
/* 1 for a key of these (car, opponent, track, look.<id>): what the mode file's writer keeps of the file as it was (port_mode_write) */
int car_looks_key(const char *key);

#endif
