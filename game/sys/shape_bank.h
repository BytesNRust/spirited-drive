/* A shape bank (an SHPI in memory): its count at +8, then a name and an
 * offset from the bank for each shape from +0x10. */
#ifndef TNFS_SHAPE_BANK_H
#define TNFS_SHAPE_BANK_H

#include <stdint.h>

/* 004732dd: the first shape whose 4-byte name is `name`, NULL when there is
 * none (the original aborts the game then; 004732e4 is the same search
 * returning NULL quietly). */
uint8_t *shape_bank_find(uint8_t *bank, const uint8_t name[4]);

#endif
