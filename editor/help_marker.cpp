// The "(?)" of Studio's panels (help_marker.h)
#include "help_marker.h"

#include "imgui.h"

void help_marker(const char *text)
{
    ImGui::TextDisabled("(?)");
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::CalcTextSize("0").x * HELP_LINE_CHARS);   // a line of at most HELP_LINE_CHARS characters
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}
