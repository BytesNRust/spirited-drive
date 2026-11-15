/* What tnfs-studio validate does with a mod (dep59 tools, stage A): a folder with a mod.ini is read through its card, and a car
 * piece is checked as the game loads it (car_piece_check); a mod's zip passes the check the installation makes (mod_check_zip),
 * and a car piece in it the piece's check too. */
#ifndef TNFS_STUDIO_MOD_VALIDATE_H
#define TNFS_STUDIO_MOD_VALIDATE_H

#include "formats/exchange.h"
#include "formats/validate.h"

/* The files of a folder (by their path below it) into `r`: with no mod.ini, validate_tree as ever; with one, its card must read (0 with
 * the reason in `err` otherwise), a car piece goes through car_piece_check and its other files through validate_tree. */
int mod_validate_tree(const FileSet *set, Report *r, ExchangeError *err);
/* A mod's zip: 0 with the reason in `err` when the installation would refuse it; else 1, a car piece's findings in `r`. */
int mod_validate_zip(const uint8_t *zip, size_t size, Report *r, ExchangeError *err);

#endif
