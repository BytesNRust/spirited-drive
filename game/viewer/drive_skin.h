/* port (dep59 step 3): an added car's skin and colour in the race. The player's added car wears the skin and paint picked in its
 * gallery (car_looks.h, by name); a rival's added car, and the player's with the Car Colours switch off, the skin as it comes and its
 * own colour. A pick the piece no longer has falls back the same way, with a line in the log. The colour applies only where the skin
 * has masks (car_pieces_colour_on), and they are read from the skin's own folder (car_masks/<STEM>/ for the skin as it comes). */
#ifndef TNFS_DRIVE_SKIN_H
#define TNFS_DRIVE_SKIN_H

#include "game/sys/car_looks.h"
#include "game/sys/car_pieces.h"

#include <stddef.h>
#include <stdint.h>

enum { DRIVE_SKIN_DIR_MAX = CAR_PIECE_DIR_MAX + 32 };

typedef struct {
    char skin_dir[DRIVE_SKIN_DIR_MAX];   /* the skin's folder; "" the skin as it comes */
    char mask_dir[DRIVE_SKIN_DIR_MAX];   /* where its masks are: the skin's folder, or the piece's car_masks/<STEM> */
    int paint_on;                        /* 1: the body takes `rgb` */
    uint8_t rgb[3];
    CarLook worn;                        /* the names of the skin and paint found ("" the skin as it comes, its own colour): what a replay keeps */
} DriveSkinLook;

/* each texture of `skin_dir` (<tex>.png high detail, <tex>_lo.png low) put over the entry of that name and size in the CFM's banks, as
 * the car's palette indices (an indexed PNG as tnfs-studio unpack writes it, taken as it is, the colours with a meaning 0xfa..0xff
 * too, as the author chose them; any other mapped to entries 0x00..0xf9 by nearest colour); the CFM is the caller's. A file of another size is passed over with a
 * line in the log. How many textures */
int drive_skin_apply(const char *skin_dir, uint8_t *cfm, size_t size);

/* the look of the piece `card` for a pick `look` (NULL: a rival's, the skin as it comes and its own colour) */
void drive_skin_choose(const CarPieceCard *card, const CarLook *look, DriveSkinLook *out);

/* the look of the registered added car `car` in this race: `recorded` when not NULL (a replay's), else the player's pick when
 * `player`, else as a rival's; 0 when `car` is not an added car or its card no longer reads */
int drive_skin_look(int car, int player, const CarLook *recorded, DriveSkinLook *out);

#endif
