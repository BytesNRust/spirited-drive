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

int fsio_exists(const char *path);

/* Removes a file; 1 also when it was already gone. */
int fsio_remove(const char *path);

/* Removes a directory tree below `root` (a plain walk: symbolic links are removed, not followed). */
int fsio_remove_tree(const char *path);

/* Removes `dir` and its parents up to (not including) `stop` while they are empty. */
void fsio_prune_empty(const char *dir, const char *stop);

/* Finds `rel` below `root` ignoring case in every component; `out` gets root/real-spelling. 0 when absent. */
int fsio_find_ci(const char *root, const char *rel, char *out, size_t cap);

/* Writes beside the target and renames, so an interrupted write cannot leave a half file. */
int fsio_write_atomic(const char *path, const uint8_t *data, size_t size, ExchangeError *err);

/* FNV-1a, 64 bits: enough to notice that a file changed. */
uint64_t fsio_hash(const uint8_t *data, size_t size);

#endif
