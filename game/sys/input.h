/* Keyboard as the game sees it. 00464bd0 reports the key held *now*, not a
 * queued press, checking a fixed list in priority order. ASCII keys return
 * their character, cursor and function keys their BIOS code (scancode << 8),
 * modifiers and the Y/N/J answers their raw scancode. 0 when none is held. */
#ifndef TNFS_INPUT_H
#define TNFS_INPUT_H

enum {
    KEY_NONE = 0,
    KEY_ENTER = 0x0D,
    KEY_ESCAPE = 0x1B,
    KEY_SPACE = 0x20,
    KEY_UP = 0x4800,
    KEY_DOWN = 0x5000,
    KEY_LEFT = 0x4B00,
    KEY_RIGHT = 0x4D00,
    KEY_INSERT = 0x5200,
    KEY_DELETE = 0x5300,
    KEY_F1 = 0x3B00,
    KEY_F2 = 0x3C00,
    KEY_F3 = 0x3D00,
    KEY_F4 = 0x3E00,
    KEY_LSHIFT = 0x2A,
    KEY_RSHIFT = 0x36,
    KEY_ALT = 0x38,
    KEY_Y = 0x15,
    KEY_N = 0x31,
    KEY_J = 0x24,
    KEY_CTRL = 0x1D
};

/* Pumps OS events; exits the process when the window is closed. */
int input_get_key(void);  /* 00464bd0 */

/* The OS events pumped, the pointer synced, 1 ms given back; a window closed
 * ends the process after input_on_close's hook. */
void input_pump(void);

/* A window closed: `hook` runs before the process ends (the front end's config.dat save); NULL none. */
void input_on_close(void (*hook)(void));

/* 0049b6fc: next typed character in the game's encoding (CP437, as its
 * fonts), 0x08 backspace, 0x0d Enter, 0x1b Escape, KEY_DELETE, or 0. */
int input_next_char(void);

#endif
