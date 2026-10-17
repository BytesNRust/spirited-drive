/* The menus' modal Yes/No box (00465f4c). */
#ifndef TNFS_YES_NO_BOX_H
#define TNFS_YES_NO_BOX_H

#include "game/frontend/message_banner.h"

#include <stdint.h>

/* The pointer as 00465f4c reads and writes it (main's pointer.c owns it). */
typedef struct DialogPointer {
    int active;          /* the short 00524790 */
    int buttons;         /* the short 00524792 */
    int mark_x, mark_y;  /* the shorts 00524794 / 00524796: where 00441858 last saw it */
    int32_t x, y;        /* 004c6110 / 004c6114 */
} DialogPointer;

typedef struct YesNoOps {
    const MessageBannerOps *draw;  /* pictures, shading, frames, text styles; its own ctx */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8: eax, edx, ebx */
    void (*text_boxed)(void *ctx, const char *text, int32_t x, int32_t y);     /* 00463934: eax, edx, ebx */
    void (*pointer_hide)(void *ctx);        /* [004c618c] */
    /* [004c6194]: reads the mouse (00477140 by default) into p->x / p->y; its answer, the buttons, dropped here */
    void (*pointer_poll)(void *ctx, DialogPointer *p);
    void (*pointer_show)(void *ctx);        /* 00441798 */
    /* 004417f8: [004c6194], the pointer redrawn where it moved (004416cc); gives the buttons */
    int32_t (*pointer_buttons)(void *ctx, DialogPointer *p);
    void (*pointer_click)(void *ctx);       /* 00441814 */
    void (*pointer_warp)(void *ctx, int32_t x, int32_t y);  /* 004772bc (cdecl) */
    void (*pointer_mark)(void *ctx, DialogPointer *p);  /* 00441858: the marks = the pointer's low halves */
    int32_t (*key)(void *ctx);              /* 00464bd0 */
    void (*music_service)(void *ctx);       /* 0046417c */
    void (*sound)(void *ctx, int32_t sound);  /* 0046492c: eax the sound */
    void (*drain)(void *ctx);               /* 004770ac */
    void *ctx;
} YesNoOps;

/* 00465f4c: a 0x154 x 0x64 box at (0x96, 0xbe) over the screen (grabbed
 * twice: one copy to restore, one to draw on), darkened, two red frames, the
 * title centred in style 2, "Yes" / "No" ("Ja" / "Nein" when german) with No
 * selected. Then, until an answer: the pointer over an option selects it;
 * each change plays 0x47 and redraws the two options on the screen; Enter,
 * space or a button (once released since the box opened) answers the
 * selection (the right button, 2, answers No), but only once a frame without
 * a key has armed the keys; Y (J), N and Escape answer at once; the arrows
 * move the selection and the pointer. Every answer plays 0x48. The screen
 * under the box is put back. Returns 1 for Yes. */
int yes_no_box(const char *title, int german, DialogPointer *p, const MessageBannerInputs *in, const YesNoOps *ops);

#endif
