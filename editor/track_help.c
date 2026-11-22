#include "track_help.h"

#include "shortcut_keys.h"

#include <stdio.h>

void track_help_text(char *out, size_t cap, int mac)
{
    char save[16], undo[16], redo[24];
    shortcut_label(save, sizeof save, mac, "S");
    shortcut_label(undo, sizeof undo, mac, "Z");
    shortcut_label(redo, sizeof redo, mac, "Shift+Z");
    snprintf(out, cap, "Mouse: left drag moves a point, right or middle drag pans, the wheel zooms.\n"
                       "Keys: arrows nudge, F fit, N add, I insert, Del remove, Esc clear the selection, %s save, %s undo, %s redo, B build, U install.", save, undo, redo);
}
