/* The studio's file system layer: whole-file reads, file trees written from and read into a FileSet.
 * Names in a FileSet are relative with '/' separators (formats/exchange.h); nothing here writes outside
 * the directory it is given, and nothing is overwritten unless asked. */
#ifndef TNFS_STUDIO_FSIO_H
#define TNFS_STUDIO_FSIO_H

#include "formats/exchange.h"

/* malloc'd contents; NULL with `err` filled on failure. */
uint8_t *fsio_read_file(const char *path, size_t *size, ExchangeError *err);

/* Writes a file, creating directories. Refuses an existing file unless `force`. */
int fsio_write_file(const char *path, const uint8_t *data, size_t size, int force, ExchangeError *err);

/* Writes every file of the set under `dir`. With !force it first checks that none exists, so a refusal
 * leaves the directory as it was. */
int fsio_write_tree(const char *dir, const FileSet *set, int force, ExchangeError *err);

/* Reads every regular file below `dir` (symbolic links are skipped) into the set. */
int fsio_read_tree(const char *dir, FileSet *set, ExchangeError *err);

#endif
