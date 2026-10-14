/* A key's name on the controls screen. */
#ifndef TNFS_KEY_NAME_H
#define TNFS_KEY_NAME_H

#include <stdint.h>

enum { KEY_NAME = 0x10 };  /* the longest name ("Vorherige", "Backspace") and its NUL fit */

/* 00465498: the name of a key (a character, or a scancode << 8): the space,
 * Enter, the arrows, the editing keys, Backspace, Tab and the keypad's centre
 * by name (in German with the 0052b31c language), other printable
 * characters as themselves (a..z as capitals in English; in German also
 * the font's 0x81, 0x84, 0x8e, 0x94, 0x99, 0x9a and 0xe1). For any other key
 * `out` is left as it was. */
void key_name_text(char out[KEY_NAME], int german, int32_t key);

#endif
