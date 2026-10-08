/* Keyboard as the game sees it: 00464bd0 returns 0 when no key is waiting,
 * otherwise the key's ASCII code (Enter 0x0d, Escape 0x1b, Space 0x20). */
#ifndef TNFS_INPUT_H
#define TNFS_INPUT_H

enum {
    KEY_NONE = 0,
    KEY_ENTER = 0x0D,
    KEY_ESCAPE = 0x1B,
    KEY_SPACE = 0x20,
    /* Extended keys: codes not yet confirmed against 00464bd0. */
    KEY_UP = 0x148,
    KEY_DOWN = 0x150,
    KEY_LEFT = 0x14B,
    KEY_RIGHT = 0x14D
};

/* Pumps OS events; exits the process when the window is closed. */
int input_get_key(void);  /* 00464bd0 */

#endif
