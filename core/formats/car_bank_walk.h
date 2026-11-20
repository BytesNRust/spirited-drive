/* port: the walk over a CFM's two banks (high and low detail) that the livery code and the car editor share. For each bank it builds the textures
 * as the repaint code (formats/car_repaint.h) reads them (pixels, width, the body class of the texture's name), lets the caller fill in the rest
 * (its mask, its kept rectangles, a body class of its own) through `texture`, and gives the bank, with the palettes, to `bank`. What a texture's
 * mask is, and where it comes from, is the caller's: the walk knows no files. */
#ifndef TNFS_CAR_BANK_WALK_H
#define TNFS_CAR_BANK_WALK_H

#include "formats/car_repaint.h"

#include <stddef.h>
#include <stdint.h>

enum { CAR_BANK_MAX_TEXTURES = 32 };

/* what the `bank` callback knows of the bank beside the texture list: its detail level (0 high, 1 low) and the textures' names, in the list's order */
typedef struct {
    int level;
    char name[CAR_BANK_MAX_TEXTURES][4];
} CarBankMeta;

typedef struct {
    /* Called for each bitmap texture of a bank, in order, with `tex` made (pixels, count, width, body) from the file; it may set tex->mask (malloc'd:
     * the walk frees it after the bank), tex->keep, tex->body. May be NULL. */
    void (*texture)(void *user, int level, const char name[4], int width, int height, RepaintTexture *tex);
    /* Called with each bank that has a palette; returns the number of changes, added to the walk's total (a negative one counts for none). */
    int (*bank)(void *user, const RepaintBank *bank, const CarBankMeta *meta);
} CarBankHooks;

/* Walks the CFM `cfm` (`size` bytes, edited in place by the callbacks) and returns the sum of the `bank` callback's results; 0 for a file that is not a CFM. */
int car_bank_walk(uint8_t *cfm, size_t size, const CarBankHooks *hooks, void *user);

#endif
