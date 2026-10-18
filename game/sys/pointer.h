/* The game's own mouse pointer: a sprite from mouse.fsh drawn over the
 * screen, a position the game may move (to keep keyboard and mouse
 * selection in sync) and a button state. In a window the port never moves
 * the OS cursor: keyboard moves only the game's pointer. */
#ifndef TNFS_POINTER_H
#define TNFS_POINTER_H

#include <stdint.h>

enum {
    POINTER_BUTTON_NONE = 0,
    POINTER_BUTTON_SELECT = 1,  /* left */
    POINTER_BUTTON_BACK = 2     /* right */
};

int pointer_load(int variant);  /* 00441530: 0 mouse.fsh, 1 gmouse.fsh */
void pointer_free(void);

void pointer_sync_from_os(void);                      /* window thread, WM_MOUSEMOVE */
void pointer_move_to(int x, int y);                   /* 00464038 */
int pointer_in_rect(int x0, int x1, int y0, int y1);  /* 00463f80 */
void pointer_activate_if_moved(void);                 /* 00464088 */
void pointer_off(void);                               /* 00464060 */
int pointer_active(void);                             /* 00524790 */
void pointer_show(void);                              /* 00441798 */
int pointer_read_buttons(void);                       /* 004417f8 */
/* 004417f8 without the pointer drawn: a movie's frames hide it in the original, the port's overlay would stay */
int pointer_buttons_unseen(void);
void pointer_click_feedback(void);                    /* 00441814 */
void pointer_position(int *x, int *y);                /* 004c6110 / 004c6114 */
void pointer_mark(void);                              /* 00441858: the marks where it is */
/* The pointer's art as 00441530 takes it for the race's (00509eb0..: "ptr0", "ptr1", "ptr2"): the raw entry of
 * "ptr<which>", NULL when it is not loaded */
const uint8_t *pointer_shape(int which);

/* The short 00524790 itself, which the screens read while show, activate
 * and off write it: one word, as the original's. */
int16_t *pointer_active_word(void);
/* A screen holding the short by value: `copy` kept equal to it while bound
 * (NULL none); the binding before is returned to restore on leaving. */
int16_t *pointer_bind_active(int16_t *copy);

#endif
