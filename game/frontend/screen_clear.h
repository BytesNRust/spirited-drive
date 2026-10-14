/* Both pages of the menus' screen cleared. */
#ifndef TNFS_SCREEN_CLEAR_H
#define TNFS_SCREEN_CLEAR_H

#include <stdint.h>

typedef struct ScreenClearOps {
    void (*select_default)(void *ctx);         /* 00473604 */
    void (*fill)(void *ctx, uint32_t colour);  /* 00482980 (cdecl): the clip rectangle filled */
    void (*unlock)(void *ctx);                 /* 004733b0 */
    void (*flip)(void *ctx);                   /* 004733ec */
    void *ctx;
} ScreenClearOps;

/* 00464a30: twice, the default picture selected, filled with 0, unlocked
 * and shown, so both pages are black. */
void screen_clear_both(const ScreenClearOps *ops);

#endif
