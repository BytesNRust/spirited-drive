/* port (dep59 step 4b, T6): the CONTENT screen's two dialogs, the port's own, over the screen darkened 45%: a panel with a yellow frame
 * (notes/mockups/content_install_prompt.png). The edition's name box: a title and the game's own text field (text_entry_edit) for a
 * name of up to EDITION_NAME_MAX, the name kept as the main face draws it (text_words), its spaces trimmed. The install prompt: "Installed:
 * <name> <version> - Add to <edition>?" (two lines when too wide: the install, then the question) with Yes and No, and the line that third-party mods are the installer's responsibility; in the
 * standard edition nothing is asked: the same line and OK. No drawing and no files here: ContentDialogOps draws (the glue's, T7 calls). */
#ifndef TNFS_EDITION_NAME_BOX_H
#define TNFS_EDITION_NAME_BOX_H

#include "formats/edition_ini.h"

#include <stdint.h>

enum {
    DIALOG_DARKEN = 45,                                                   /* per cent */
    DIALOG_X0 = 110, DIALOG_Y0 = 176, DIALOG_X1 = 530, DIALOG_Y1 = 304,   /* the panel, ends included */
    DIALOG_CX = (DIALOG_X0 + DIALOG_X1) / 2, DIALOG_TEXT_W = DIALOG_X1 - DIALOG_X0 - 40,
    DIALOG_LINE_Y = 196, DIALOG_NOTE_Y = 226, DIALOG_FIELD_Y = 230,
    DIALOG_TWO_Y0 = 186, DIALOG_TWO_Y1 = 208, DIALOG_TWO_NOTE_Y = 233,   /* the prompt on two lines: too wide for one at 5/6 */
    DIALOG_BUTTON_Y0 = 258, DIALOG_BUTTON_Y1 = 289, DIALOG_BUTTON_W = 84,
    DIALOG_YES_X0 = 196, DIALOG_NO_X0 = 360, DIALOG_OK_X0 = DIALOG_CX - DIALOG_BUTTON_W / 2,
    /* the text's styles (text.h's) */
    DIALOG_CREAM = 0, DIALOG_YELLOW = 1, DIALOG_DIM = 4,
    /* the keys it takes (the front end's) */
    DIALOG_KEY_ENTER = 0x0d, DIALOG_KEY_ESCAPE = 0x1b, DIALOG_KEY_LEFT = 0x4b00, DIALOG_KEY_RIGHT = 0x4d00
};

extern const char DIALOG_THIRD_PARTY[];   /* the line under an install's */

typedef struct {
    void (*backdrop)(void *ctx, int32_t percent);   /* the screen as it was when the dialog opened, darkened `percent` per cent */
    void (*panel)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1);   /* the panel's dark ground */
    void (*frame)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t lit);   /* a frame, yellow when lit, dim else */
    void (*text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den);
    int32_t (*text_width)(void *ctx, const char *text, int32_t num, int32_t den);
    /* text_entry_edit's field centred on x for up to `max` characters, free text: 1 taken with Enter, 0 left with Escape */
    int32_t (*text_field)(void *ctx, char *text, int32_t max, int32_t x, int32_t y);
    void (*show)(void *ctx);
    int32_t (*key)(void *ctx);                                   /* the key held, 0 for none */
    int32_t (*pointer)(void *ctx, int32_t *x, int32_t *y);       /* the buttons held (1 the left one), the place */
    void (*wait)(void *ctx);                                     /* a frame */
    void (*close)(void *ctx);                                    /* the screen as it was put back */
    void *ctx;
} ContentDialogOps;

/* the install's prompt; `edition` NULL for the standard edition (OK only). 1 Yes (or OK), 0 No or Escape */
int32_t content_install_prompt(const ContentDialogOps *o, const char *name, const char *version, const char *edition);

/* the edition's name box under `title`, `name` (EDITION_NAME_MAX + 1) its start and what it becomes; 1 taken, 0 left as it was (Escape,
 * or nothing left once cleaned) */
int32_t edition_name_box(const ContentDialogOps *o, const char *title, char *name);

#endif
