// A line of text kept to the width it has (Studio's font is monospaced): too long, it keeps its start and its end with "..." in the middle (path_elide.h), never breaking a word
// across lines, and the whole of it is in the line's tooltip.
#ifndef EDITOR_TEXT_ELIDED_H
#define EDITOR_TEXT_ELIDED_H

// `text` on the current line in at most `room` pixels (the rest of the line when room <= 0), in the style colour now in force
void text_elided(const char *text, float room = 0.0f);

// the same as ImGui::Text("%s%s", label, text) with the room left after the label
void text_elided_after(const char *label, const char *text);

#endif
