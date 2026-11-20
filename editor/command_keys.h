/* The Studio's command keys, Save, Undo and Redo, on the platform's command key (Cmd on a Mac, Ctrl elsewhere: shortcut_keys.h), the same in
 * every editor. A plain letter is never one of them, so typing in a field cannot save (owner 2026-11-20). Dear ImGui, inside a window. */
#ifndef TNFS_TRACK_EDITOR_COMMAND_KEYS_H
#define TNFS_TRACK_EDITOR_COMMAND_KEYS_H

enum CommandKey { COMMAND_NONE, COMMAND_SAVE, COMMAND_UNDO, COMMAND_REDO };

/* The command pressed this frame: Cmd+S Save; Cmd+Z Undo; Cmd+Shift+Z or Cmd+Y Redo. While a text field has the keyboard (`text_input`) only Save:
 * the field keeps its own Cmd+Z. Called every frame, so the keys' routes are kept. */
CommandKey command_key(bool text_input);

#endif
