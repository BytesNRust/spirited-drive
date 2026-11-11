/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: a cache, in the player's folder (<user>/cache/), of the pictures the port makes from the CD's art (the credits' relief, the boot screen, the
 * configuration screens' clean relief): made once, read back on the next runs. An entry is made again only when its key changes: the generator's own
 * version (a constant raised by hand when its algorithm changes) and the size and hash of every file it was made from (the CD's, the port's data/). A
 * missing, foreign or damaged entry is made again and rewritten, with a line in the log; a folder that cannot be written leaves it made in memory as
 * before. The cache never makes a run fail. Its folder is not one the profile's Restore touches.
 * File: "SDAC", u32 format, u64 key, u32 size, u32 FNV-1a of the bytes, the bytes (little endian). Written beside and renamed, so a crash leaves no half. */
#ifndef TNFS_ART_CACHE_H
#define TNFS_ART_CACHE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t h;
} ArtCacheKey;

/* The folder the entries go in, with its trailing separator ("<user>/cache/"); made when the first entry is written. NULL or "": no cache (the tests,
 * the tools), every read misses and nothing is written. */
void art_cache_set_dir(const char *dir);

/* A key for `generator` at `version`, then each input added: its size and bytes (NULL data: an input that is not there, which keys as itself). */
void art_cache_key_init(ArtCacheKey *key, const char *generator, uint32_t version);
void art_cache_key_add(ArtCacheKey *key, const void *data, size_t size);

/* Entry `name` ("boot.bin") into out (size bytes) when it is there, has this key and this size and its bytes are whole: 1. Else 0, with a log line saying
 * why when there was a file. */
int art_cache_read(const char *name, const ArtCacheKey *key, void *out, size_t size);

/* Writes entry `name`; 1 when it was written. */
int art_cache_write(const char *name, const ArtCacheKey *key, const void *data, size_t size);

#endif
