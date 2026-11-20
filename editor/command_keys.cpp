#include "command_keys.h"

#include "imgui.h"

CommandKey command_key(int text_input)
{
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) return COMMAND_SAVE;   // ImGuiMod_Ctrl is Cmd on a Mac
    if (text_input) return COMMAND_NONE;
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal))
        return COMMAND_REDO;
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) return COMMAND_UNDO;
    return COMMAND_NONE;
}
