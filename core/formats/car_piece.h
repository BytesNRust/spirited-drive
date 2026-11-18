/* port (dep59): the rules of a car piece, a mod of type=car (notes/dep59-content-model-design.md), one for the game and the studio. */
#ifndef TNFS_CAR_PIECE_H
#define TNFS_CAR_PIECE_H

#include "formats/exchange.h"
#include "formats/mod_ini.h"
#include "formats/validate.h"

enum { CAR_PIECE_FILES = 3, CAR_PIECE_PATH = 64, CAR_PIECE_DIR_MAX = 512, CAR_PIECE_SKINS_MAX = 15, CAR_PIECE_SKIN_NAME_MAX = 16 };   /* CAR_PIECE_DIR_MAX: a piece's folder, with the NUL (dep59 step 3) */
/* port (dep59 step 3): a piece's pictures, each of its exact size (the game does not scale): Control Central's card, the head to head
 * rival's card, the gallery's picture; a skin's mask is no larger than CAR_PIECE_MASK_MAX_SIDE a side */
enum { CAR_PIECE_CARD_W = 309, CAR_PIECE_CARD_H = 166, CAR_PIECE_RIVAL_W = 284, CAR_PIECE_RIVAL_H = 117, CAR_PIECE_GALLERY_W = 320,
       CAR_PIECE_GALLERY_H = 200, CAR_PIECE_MASK_MAX_SIDE = 1024,
       CAR_PIECE_PHOTO_MIN_W = 100, CAR_PIECE_PHOTO_MIN_H = 60, CAR_PIECE_PHOTO_MAX_W = 1280, CAR_PIECE_PHOTO_MAX_H = 960,   /* the showcase's photos (dep59), cropped to fill */
       CAR_PIECE_SKIN_FILES_MAX = 256,           /* a skin's files: four for each texture of the two banks at most */
       CAR_PIECE_PICTURE_BYTES_MAX = 8 << 20 };  /* a picture's file, read from a folder (a zip's are its own limits) */   /* the mask: an installed mod's pictures' side (MOD_PNG_MAX_SIDE) */
/* where the PBS's fields the check reads lie (docs/car_specs.tsv; the game's CarSpecs is tied to them, car_registry.c) */
enum { CAR_PIECE_PBS_POWER = 0x03c, CAR_PIECE_PBS_TORQUE_POINTS = 0x088, CAR_PIECE_PBS_TORQUE_CURVE = 0x0b8, CAR_PIECE_PBS_SHIFT_RPM = 0x288,
       CAR_PIECE_PBS_UNKNOWN_320 = 0x320, CAR_PIECE_PBS_FORCE_SCALE = 0x328,
       /* the race walks the curve 8 bytes a point from +0x0b8 (00409a04, car_drive.c) and scales each point in place: the room before
        * shift_rpm. The CD's cars have 51 (the 911, 41); 0 reads the point before the curve */
       CAR_PIECE_TORQUE_POINTS_MIN = 1, CAR_PIECE_TORQUE_POINTS_MAX = (CAR_PIECE_PBS_SHIFT_RPM - CAR_PIECE_PBS_TORQUE_CURVE) / 8 };

/* "supra" "diablo" "911" "zr1" "512tr" "viper" "nsx" "rx7" -> the race model 0..7; -1 otherwise */
int car_piece_model_of_word(const char *word);
/* the word of a race model 0..7, NULL otherwise */
const char *car_piece_word_of_model(int model);
/* 1 when `stem` may name an added car's files: 1..8 of A-Z 0-9, not a stem of the CD's cars, traffic or police (slots.h, and P911T, the
 * race's ninth), not 'M' + one (the reduced model's), whatever the case */
int car_piece_stem_ok(const char *stem);
/* the three files of the car `stem`, as paths below the piece's root: its CFM, its PDN, its PBS */
void car_piece_paths(const char *stem, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH]);
/* port (dep59 step 3): 1 when `rel` is one of a car piece's own pictures, by these names in lower case as the game reads them:
 * card.png, rival.png, gallery.png, or a PNG of a folder of skins/ (skins/<name>/<file>.png; which names and files are a skin's is
 * car_piece_check's PIECE.SKIN to say) */
int car_piece_picture(const char *rel);
/* (port, dep59 showcase: a PNG of the piece's root named as a photo is one of its pictures too, mod_ini_photo_name_ok; car_piece_check's PIECE.PHOTO
 * says whether photos= and photo_small= name every one and each is a readable PNG of PHOTO_MIN to PHOTO_MAX a side) */
/* 1 when `name` may name a skin's folder: 1..CAR_PIECE_SKIN_NAME_MAX of a-z 0-9 '-' */
int car_piece_skin_name_ok(const char *name, size_t n);
/* Everything the game asks of a car piece before it lets it race: the card's fields (PIECE.TYPE, PIECE.STEM, PIECE.FROM), the three
 * files (PIECE.FILE_MISSING; found whatever their case), each one's own limits (validate_file) and the specs' (PIECE.SPECS); its
 * pictures (PIECE.CARD: card.png there and 309 x 166; PIECE.PICTURE: rival.png and gallery.png of their sizes when there) and its skins
 * (PIECE.SKIN: up to CAR_PIECE_SKINS_MAX folders of a skin's name, each file <tex>[_lo][_mask].png of a texture the CFM's high (low,
 * _lo) bank has, of its size; a mask a readable PNG). Findings go to `r`; 1 when none of them is fatal. `files` holds the piece's files
 * by their path below its root. */
int car_piece_check(const ModInfo *card, const FileSet *files, Report *r);

#endif
