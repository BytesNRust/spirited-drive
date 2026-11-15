/* port (dep59 step 3): a car piece's card.png, the picture Control Central shows for an added car (309 x 166).
 *
 *   tnfs-studio car card PIECE_DIR --generic [--force]
 *   tnfs-studio car card PIECE_DIR --tnfs BIN [--data DIR] [--track TR1] [--node N] [--time day|dawn|dusk|night] [--force]
 *
 * The generic card is a dark grey ground, a 2-pixel frame and the card's name (mod.ini `name`) in the port's own 5x7 bitmap font
 * (printable ASCII; a character it lacks is skipped), as large as fits. Nothing of the CD. `car new` writes one; `car card --generic`
 * makes it again (a piece installed before step 3 has none), refusing an existing card.png without --force.
 *
 * Without --generic the pictures come from the game (studio/car_card_shot.c): the game races the piece and its frame is cropped,
 * centred on the car, to card.png (309 x 166), rival.png (284 x 117) and gallery.png (320 x 200). The track, the node and the time of
 * day choose the background. Without --force it replaces only a card.png that is still the generic one. */
#ifndef TNFS_STUDIO_CAR_CARD_H
#define TNFS_STUDIO_CAR_CARD_H

#include "formats/exchange.h"
#include "formats/mod_ini.h"

#include <stddef.h>
#include <stdint.h>

enum { CAR_CARD_W = 309, CAR_CARD_H = 166 };

/* the generic card of a car named `name`, as PNG bytes (malloc'd); 0 with `err` filled */
int car_card_generic_png(const char *name, uint8_t **png, size_t *len, ExchangeError *err);
/* PIECE_DIR/card.png from its mod.ini's name; an existing one only with `force`. 0 with `err` filled */
int car_card_generic(const char *piece_dir, int force, ExchangeError *err);
typedef struct {
    const char *piece, *tnfs, *data, *track, *time_name;
    int node, force;
} CarCardShot;

/* the piece's card (mod.ini), a car's; 0 with `err` filled */
int car_card_read_info(const char *piece_dir, ModInfo *info, ExchangeError *err);
/* card.png, rival.png and gallery.png of the piece from the game's frame (studio/car_card_shot.c); 0 with `err` filled */
int car_card_shot(const CarCardShot *s, ExchangeError *err);
/* argv[0] is "car", argv[1] "card". 0 with `err` filled on failure. */
int car_card_command(int argc, char **argv, ExchangeError *err);

#endif
