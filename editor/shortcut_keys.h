/* The Studio's shortcuts as the menus name them (owner test U1): the platform's command key, Cmd on macOS and Ctrl elsewhere. The keys themselves go
 * through Dear ImGui's ImGuiMod_Ctrl, which is Cmd on a Mac (io.ConfigMacOSXBehaviors swaps them; tests/test_editor_shortcuts.cpp). Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_SHORTCUT_KEYS_H
#define TNFS_TRACK_EDITOR_SHORTCUT_KEYS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
#define SHORTCUT_MAC 1
#else
#define SHORTCUT_MAC 0
#endif

/* "Cmd" when `mac`, else "Ctrl" */
const char *shortcut_mod_name(int mac);
/* the command key and `keys` ("Shift+Z") as one label ("Cmd+Shift+Z") into `out`; 0 with `out` "" when it does not fit */
int shortcut_label(char *out, size_t cap, int mac, const char *keys);

#ifdef __cplusplus
}
#endif

#endif
