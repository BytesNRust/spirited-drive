/* The track editor's help for the whole interface: what the mouse and the keys do. One "(?)" in the Viewport's corner shows it. Pure C. */
#ifndef EDITOR_TRACK_HELP_H
#define EDITOR_TRACK_HELP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the text into `out` (cut to fit, always ended); `mac` names the command key Cmd, else Ctrl */
void track_help_text(char *out, size_t cap, int mac);

#ifdef __cplusplus
}
#endif

#endif
