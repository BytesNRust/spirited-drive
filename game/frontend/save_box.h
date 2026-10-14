/* The "Save game as" box. */
#ifndef TNFS_SAVE_BOX_H
#define TNFS_SAVE_BOX_H

#include "game/frontend/message_banner.h"

#include <stdint.h>

/* the original's path buffer (esp + 0x100, below the two pictures): dir + name + ".SAV" must fit */
enum { SAVE_BOX_PATH = 0x28 };

typedef struct SaveBoxOps {
    const MessageBannerOps *draw;  /* pictures, shading, frames, text styles; its own ctx */
    /* 004626d0: the text field (eax the text, edx its length, ebx x, ecx y, then 0 on the stack) */
    int32_t (*text_field)(void *ctx, char *text, int32_t length, int32_t x, int32_t y, int32_t zero);
    void (*sound)(void *ctx, int32_t sound);                         /* 0046492c: eax the sound */
    int32_t (*file_exists)(void *ctx, const char *path);             /* 004886e0 (cdecl) */
    int16_t (*ask)(void *ctx, const char *text);                     /* 00465f4c (yes_no_box): eax */
    void (*remove_file)(void *ctx, const char *path);                /* 0048855b: eax */
    int32_t (*save)(void *ctx, const char *path);                    /* 0042a580 (game_file_save): eax */
    void (*banner_wait)(void *ctx, const char *text, int32_t time);  /* 00467214 (message_banner_wait) */
    void (*pointer_redraw)(void *ctx, int32_t eax, int32_t edx);     /* 004416cc: eax 0, edx 1 */
    void *ctx;
} SaveBoxOps;

typedef struct SaveBoxState {
    uint8_t *config;     /* 00524df0: config.dat's image (the name at +9, the controls at +0x5f36) */
    uint8_t *game;       /* 0052ad44: the game state */
    int16_t lost_vegas;  /* the short 004baf18 */
    const char *dir;     /* 005241e8: paths.dat's slot 1, the saved games' */
} SaveBoxState;

/* 0042a680: a 0x190 x 0x64 box at 0x78, 0xbe over the screen darkened
 * (0x9000), red frames, "Save game as" ("Spiel speichern als") centred at
 * 0xc8, 0xa; the 8-character field at 0x140, 0xfa; the screen put back.
 * Taken: the name into config.dat's name (the original sprintf's it with
 * the name as the format: names with a '%' differ), the controls (0x1a
 * bytes) and the image's 0xac bytes from the name on into the game state
 * (0052b298, 0052b1ec), 004baf18 into 0052b2b2; dir + name + ".SAV" saved
 * (0042a580), first asking "Replace game ?" when it exists (and removing
 * it on yes; no means no save); "Game saved" for 100 ticks when it
 * worked. Sound 0x48 when the name is given up or the file is new. */
void save_box_show(char *name, int german, const MessageBannerInputs *in, SaveBoxState *s, const SaveBoxOps *ops);

#endif
