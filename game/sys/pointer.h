/* The game's own mouse pointer: a sprite from mouse.fsh drawn over the
 * screen, a position the game may move (to keep keyboard and mouse
 * selection in sync) and a button state. In a window the port never moves
 * the OS cursor: keyboard moves only the game's pointer. */
#ifndef TNFS_POINTER_H
#define TNFS_POINTER_H

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
void pointer_click_feedback(void);                    /* 00441814 */

#endif
