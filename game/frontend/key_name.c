#include "game/frontend/key_name.h"

#include <stddef.h>
#include <stdio.h>

typedef struct KeyName {
    int32_t key;
    const char *name;
} KeyName;

/* 004b16d0..004b1750 */
static const KeyName GERMAN[] = {
    { 0x20, "Leertaste" }, { 0xd, "Enter" }, { 0x5200, "Einfugen" }, { 0x5300, "L\x94schen" },
    { 0x4700, "Pos 1" }, { 0x4f00, "Ende" }, { 0x4900, "Nachste" }, { 0x5100, "Vorherige" },
    { 0x4800, "Hoch" }, { 0x5000, "Runter" }, { 0x4b00, "Links" }, { 0x4d00, "Rechts" },
    { 8, "Backspace" }, { 9, "Tab" }, { 0x4c00, "Center" }
};

/* 004b175c..004b17a4 */
static const KeyName ENGLISH[] = {
    { 0x20, "Space" }, { 0xd, "Enter" }, { 0x5200, "Insert" }, { 0x5300, "Delete" },
    { 0x4700, "Home" }, { 0x4f00, "End" }, { 0x4900, "Pgup" }, { 0x5100, "Pgdown" },
    { 0x4800, "Up" }, { 0x5000, "Down" }, { 0x4b00, "Left" }, { 0x4d00, "Right" },
    { 8, "Backspace" }, { 9, "Tab" }, { 0x4c00, "Center" }
};

/* the font's accented letters the German names take as themselves */
static const int32_t GERMAN_EXTRA[] = { 0x81, 0x9a, 0x99, 0x94, 0x8e, 0x84, 0xe1 };

/* 00465498 */
void key_name_text(char out[KEY_NAME], int german, int32_t key)
{
    enum { FIRST = 0x21, LAST = 0x7e };
    const KeyName *names = german ? GERMAN : ENGLISH;
    for (size_t i = 0; i < sizeof GERMAN / sizeof GERMAN[0]; i++)
        if (names[i].key == key) {
            snprintf(out, KEY_NAME, "%s", names[i].name);
            return;
        }
    if (key >= FIRST && key <= LAST) {
        snprintf(out, KEY_NAME, "%c", !german && key >= 'a' && key <= 'z' ? key - 0x20 : key);
        return;
    }
    if (!german) return;
    for (size_t i = 0; i < sizeof GERMAN_EXTRA / sizeof GERMAN_EXTRA[0]; i++)
        if (GERMAN_EXTRA[i] == key) snprintf(out, KEY_NAME, "%c", key);
}
