/* The car's card picture made from the Model window (dep59 car editor, «Use as card photo»): the scene as the Model's camera sees it, 309 x 166 (PIECE.CARD:
 * the game does not scale it) over a plain backdrop, written as <piece>/card.png (RGBA, all opaque; the game fits it to Central's palette when it installs). */
#ifndef TNFS_TRACK_EDITOR_CARD_PHOTO_H
#define TNFS_TRACK_EDITOR_CARD_PHOTO_H

#include "model_scene.h"

#include <stddef.h>
#include <stdint.h>

enum { CARD_PHOTO_W = 309, CARD_PHOTO_H = 166 };   /* CAR_PIECE_CARD_W / H (formats/car_piece.h) */

/* 1 when <piece>/card.png is there (a file): the panel asks before it is replaced */
int card_photo_exists(const char *piece);

/* The scene as `o` sees it (in the light `night` says, NULL: day) over `backdrop` (RGB), written to <piece>/card.png: staged as card.png.tmp and renamed, so the
 * card is the old one or the new one, never half. 1, or 0 with `why` (English, for the person; nothing is left behind). */
int card_photo_write(ModelScene *scene, const Orbit *o, const CarNightState *night, const uint8_t backdrop[3], const char *piece, char *why, size_t why_cap);

#endif
