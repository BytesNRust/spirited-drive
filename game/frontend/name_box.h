/* The "Please Enter Name" box. */
#ifndef TNFS_NAME_BOX_H
#define TNFS_NAME_BOX_H

#include "game/frontend/message_banner.h"

#include <stdint.h>

typedef struct NameBoxOps {
    const MessageBannerOps *draw;  /* pictures, shading, frames, text styles; its own ctx */
    void (*pointer_hide)(void *ctx);  /* [004c618c] */
    /* 004626d0: the text field (eax the text, edx its length, ebx x, ecx y, then 1 on the stack) */
    int32_t (*text_field)(void *ctx, char *text, int32_t length, int32_t x, int32_t y, int32_t one);
    void (*sound)(void *ctx, int32_t sound);  /* 0046492c: eax the sound */
    void *ctx;
} NameBoxOps;

typedef struct NameBoxState {
    int32_t *keys_506;  /* the dword 0052b306 (bit 3 TURBO, Open), cleared when the name is taken */
    int32_t *keys_50a;  /* the dword 0052b30a, cleared with it */
} NameBoxState;

/* 004667fc: a 0x1e0 x 0x64 box at 0x50, 0xbe: the screen under it kept,
 * darkened (0x6000) through a shade table of the screen's palette, two red
 * frames, "Please Enter Name" ("Bitte Namen Eingeben") centred in style 2
 * at 0xf0, 0xa; then the 8-character field at 0x140, 0xe6 (004626d0),
 * sound 0x48, and when its short answer is set, 0052b306 / 0052b30a
 * cleared; the screen under put back. Returns the field's answer. */
int32_t name_box_show(char *name, int german, const MessageBannerInputs *in, NameBoxState *s,
                      const NameBoxOps *ops);

#endif
