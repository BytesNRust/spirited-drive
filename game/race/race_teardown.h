/* The race's teardown frees (00455a5c's end): each frees what a setup
 * allocated through 00475260 and clears the pointer it was held in. The
 * port's owners free their own, so --drive runs none of them; they are
 * here as the exe has them. */
#ifndef TNFS_RACE_TEARDOWN_H
#define TNFS_RACE_TEARDOWN_H

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*free)(void *ctx, uint32_t block);   /* 00475260 */
} RaceFreeOps;

/* 0040d6f4 (watcall ()): free the scenery state buffer (004b9098), then
 * the pointer = 0. */
void race_scenery_state_free(const RaceFreeOps *ops, uint32_t *scenery_state);

#endif
