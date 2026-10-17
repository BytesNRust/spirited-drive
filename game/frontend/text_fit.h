/* A shadowed text cut to a width. */
#ifndef TNFS_TEXT_FIT_H
#define TNFS_TEXT_FIT_H

#include <stdint.h>

typedef struct TextFitOps {
    int32_t (*width)(void *ctx, const char *text);  /* 004770f4 (cdecl) */
    /* 0048ec60 (memmove) of 0x1040 bytes from [0052ec78] into the shadow's
     * ramp (0051fd98) or the current one (0051dd18). */
    void (*ramp)(void *ctx, int shadow);
    void (*draw)(void *ctx, const char *text, int32_t x, int32_t y);  /* 0048c264 (cdecl) */
    void *ctx;
} TextFitOps;

/* 00463d34: the first n characters of text for n from 7 on, stopping at the
 * first n whose width passes `width` (so the text kept is one character too
 * wide) or at 49; drawn with the shadow's ramp at x + 2, y + 2, then with
 * the current one at x, y. */
void text_fit_shadowed(const char *text, int32_t x, int32_t y, int32_t width, const TextFitOps *ops);

#endif
