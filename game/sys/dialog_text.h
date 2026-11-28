/* port: the text of the first start's message boxes (profile, CD or ISO, unexpected folder). Windows' message box does not wrap: a line as long as the text is, and one
 * very long line makes the window as wide as the screen; its buttons have a fixed width, so a long label is cut. So no line is longer than DIALOG_LINE_MAX characters
 * and no button label longer than DIALOG_BUTTON_MAX. No SDL: first_run_sdl3.c shows them, tests pin them. */
#ifndef TNFS_DIALOG_TEXT_H
#define TNFS_DIALOG_TEXT_H

#include <stddef.h>

enum { DIALOG_LINE_MAX = 60, DIALOG_BUTTON_MAX = 14 };

/* `text` into `out` (NUL ended, cut to `size`) with a newline where a line would pass `width` characters: at a space when there is one, inside a word (or a path) when there is not.
 * The newlines the text has stay. UTF-8 sequences are never split. Returns the length without the NUL. */
int dialog_wrap(const char *text, int width, char *out, size_t size);

/* the characters (not bytes) of a UTF-8 string */
int dialog_chars(const char *text);

/* the labels (a Windows button is DIALOG_BUTTON_MAX characters at the most: the ellipsis is one) and the start window's text. Linux writes the ellipsis as three dots:
 * SDL's own X11 box, the one without zenity, has none in its font */
#define FIRST_RUN_BUTTON_CD_LINUX "CD or folder..."
#define FIRST_RUN_BUTTON_ISO_LINUX "ISO image..."
#ifdef __linux__
#define FIRST_RUN_BUTTON_CD FIRST_RUN_BUTTON_CD_LINUX
#define FIRST_RUN_BUTTON_ISO FIRST_RUN_BUTTON_ISO_LINUX
#else
#define FIRST_RUN_BUTTON_CD "CD or folder\xe2\x80\xa6"
#define FIRST_RUN_BUTTON_ISO "ISO image\xe2\x80\xa6"
#endif
#define FIRST_RUN_BUTTON_QUIT "Quit"
#define FIRST_RUN_BUTTON_AGAIN "Choose again"
#define FIRST_RUN_BUTTON_OTHER_FOLDER "Other folder"
#define FIRST_RUN_BUTTON_OTHER_IMAGE "Other image"
#define FIRST_RUN_BUTTON_ANYWAY "Use anyway"
/* shown when no file chooser could be opened at all (what SDL said goes in the log only) */
#define FIRST_RUN_NO_CHOOSER_TEXT_LINUX                                  \
    "Your system has no file chooser this game can open.\n\n"            \
    "Install zenity with your package manager, then start again:\n"      \
    "sudo apt install zenity (Ubuntu, Mint, Zorin, elementary)\n"        \
    "sudo dnf install zenity (Fedora)\n\n"                               \
    "Or start the game from a terminal with --data and the folder that has the game's files."
#ifdef __linux__
#define FIRST_RUN_NO_CHOOSER_TEXT FIRST_RUN_NO_CHOOSER_TEXT_LINUX
#else
#define FIRST_RUN_NO_CHOOSER_TEXT "The window to choose the files could not be opened. Start the game from a terminal with --data and the folder that has the game's files."
#endif
#define FIRST_RUN_START_TEXT                                                                                         \
    "This game needs the files of your own copy of The Need for Speed Special Edition (PC, 1996).\n\n"              \
    "Choose where they are:\n"                                                                                         \
    "CD or folder: the CD, or a copy of it on your disk.\n"                                                           \
    "ISO image: an ISO image of it.\n\n"                                                                              \
    "They are copied once into this game's own folder, which takes a minute."

#endif
