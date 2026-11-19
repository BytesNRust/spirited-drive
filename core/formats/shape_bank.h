/* A shape bank (an SHPI in memory): its count at +8, then a name and an
 * offset from the bank for each shape from +0x10. */
#ifndef TNFS_SHAPE_BANK_H
#define TNFS_SHAPE_BANK_H

#include <stddef.h>
#include <stdint.h>

/* port: 1 when the `size` bytes at `bank` are a shape bank the search and the game's readers stay within: an SHPI whose
 * directory and every shape's 16-byte header fit, each bitmap's pixels fit, each palette has a '!' name and its 768 bytes, and a
 * shape named "!pal" (read as a palette whatever its type) has them too; 0 for anything else, or a shape of a type the game does not
 * draw. So a shape found by a name without '!' is a bitmap whose pixels are there. Every bank read from a file passes here before it
 * is searched (a mod's art comes from anywhere); the CD's all pass. */
int shape_bank_valid(const uint8_t *bank, size_t size);

/* 004732dd: the first shape whose 4-byte name is `name`, NULL when there is
 * none (the original aborts the game then; 004732e4 is the same search
 * returning NULL quietly) or no bank. The bank must have passed
 * shape_bank_valid. */
uint8_t *shape_bank_find(uint8_t *bank, const uint8_t name[4]);

#endif
