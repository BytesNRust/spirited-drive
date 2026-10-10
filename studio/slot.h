/* tnfs-studio slot ...: replacing the files of a track or car slot in a mod directory, and undoing it.
 *
 *   tnfs-studio slot list track|car
 *   tnfs-studio slot replace track|car SLOT SOURCE_DIR --mods DIR [--data DIR] [--stem OLD] [--also REL]... [--force]
 *   tnfs-studio slot undo --mods DIR [--all] [--force]
 *
 * SOURCE_DIR is laid out like the CD (SIMDATA/MISC/TR3.TRI, ...). Level 1 replaces files the game already
 * reads: every file must be one of the slot's own (formats/slots.h) or named with --also, must exist in the data
 * root (default "original"), and must pass validate in compatible mode. --stem OLD renames files that start with
 * OLD (and MOLD) to the slot's stem first. Files are written only below DIR; what they replace is saved in
 * DIR/.tnfs-studio so that `slot undo` leaves DIR exactly as it was. */
#ifndef TNFS_STUDIO_SLOT_H
#define TNFS_STUDIO_SLOT_H

#include "formats/exchange.h"

/* argv[0] is "slot". 0 with `err` filled on failure. */
int slot_command(int argc, char **argv, ExchangeError *err);

#endif
