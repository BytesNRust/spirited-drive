// A line of text kept to its width (text_elided.h)
#include "text_elided.h"

#include "imgui.h"
#include "path_elide.h"

#include <stdio.h>
#include <string.h>

namespace {
enum { SHOWN_MAX = 1100 };   // the buffer of the shortened line (a path is APP_PATH at most)
}

void text_elided(const char *text, float room)
{
    if (room <= 0.0f) room = ImGui::GetContentRegionAvail().x;
    const float glyph = ImGui::CalcTextSize("0").x;
    char shown[SHOWN_MAX];
    path_elide(text, glyph > 0 ? static_cast<int>(room / glyph) : static_cast<int>(strlen(text)), shown, sizeof shown);
    ImGui::TextUnformatted(shown);
    if (strcmp(shown, text) != 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
}

void text_elided_after(const char *label, const char *text)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0, 0);
    text_elided(text);
}
