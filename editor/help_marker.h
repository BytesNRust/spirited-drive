// The "(?)" of Studio's panels (notes/dep84-studio-ui-kit-draft.md section 3): a panel shows its controls and its state; the explanation of how a thing works
// lives behind a dimmed "(?)" at the end of the line it explains, shown on hover. Warnings, errors, results and state stay in sight, never in here.
#ifndef EDITOR_HELP_MARKER_H
#define EDITOR_HELP_MARKER_H

// a dimmed "(?)" on the current line (after SameLine, as the caller places it); hovered, `text` in a tooltip wrapped at HELP_LINE_CHARS characters
void help_marker(const char *text);

enum { HELP_LINE_CHARS = 90 };

#endif
