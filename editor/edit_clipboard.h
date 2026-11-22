/* The Edit menu's Cut, Copy, Paste and Select all (owner 2026-11-21, item 3), for the text field the user was in. Dear ImGui's own text fields already take Cmd+X, C, V and A
 * (Ctrl elsewhere); a click on the menu takes the keyboard from the field, so the menu cannot act on it directly. This remembers the text field that was active and its
 * selection (edit_clipboard_frame, every frame at the top of the menu bar), and an item given to edit_clipboard_do gives the field back to the user's last place: the field
 * is activated again, its selection is put back, and the very key chord the user would have typed is sent to it, so Cut, Copy and Paste are ImGui's own (the clipboard,
 * the undo of the field). No window of its own. */
#ifndef TNFS_TRACK_EDITOR_EDIT_CLIPBOARD_H
#define TNFS_TRACK_EDITOR_EDIT_CLIPBOARD_H

typedef enum { EDIT_CUT, EDIT_COPY, EDIT_PASTE, EDIT_SELECT_ALL } EditAction;

/* every frame, before the frame's text fields are drawn: remembers the active text field, and carries a queued action one step on */
void edit_clipboard_frame(void);
/* 1 when a text field has been active: the items have something to act on */
int edit_clipboard_ready(void);   /* a field was used and not left since: a click anywhere else but the menus forgets it */
/* the action on that field, over the next few frames; ignored when none was active or an action is under way */
void edit_clipboard_do(EditAction action);
/* 1 while an action is under way (a test waits for it) */
int edit_clipboard_busy(void);
/* forgets the field (a test that builds another context) */
void edit_clipboard_reset(void);

#endif
