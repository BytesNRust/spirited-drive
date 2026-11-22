/* The track editor's one help for the whole interface (owner feedback T10): a "(?)" in the Viewport's corner, with the mouse and the keys. */
#include "track_help.h"
#include "test.h"

#include <string.h>

int main(void)
{
    char text[512];
    track_help_text(text, sizeof text, 1);
    CHECK(strstr(text, "Mouse:") != NULL && strstr(text, "Keys:") != NULL);
    CHECK(strstr(text, "Cmd+S save") != NULL && strstr(text, "Cmd+Z undo") != NULL && strstr(text, "Cmd+Shift+Z redo") != NULL);
    track_help_text(text, sizeof text, 0);
    CHECK(strstr(text, "Ctrl+S save") != NULL && strstr(text, "Cmd") == NULL);
    CHECK(strstr(text, "wheel zooms") != NULL && strstr(text, "arrows nudge") != NULL);
    CHECK(strstr(text, "Esc") != NULL);   /* Esc clears the selection (the track's Esc) */
    track_help_text(text, 20, 1);   /* no room: cut, never overrun, always ended */
    CHECK(strlen(text) < 20);
    return TEST_REPORT();
}
