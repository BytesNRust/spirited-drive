// The Studio's shortcuts use the platform's command key (owner test U1): Cmd on macOS, Ctrl elsewhere. The menus name that key (editor/shortcut_keys.c), and
// Dear ImGui, which the shortcuts go through, takes the same key: on a Mac the backend's Cmd arrives as ImGuiMod_Ctrl (io.ConfigMacOSXBehaviors swaps
// them), so ImGuiMod_Ctrl | Z is Cmd+Z there and Ctrl+Z elsewhere. No window: a context fed the key events the SDL backend sends.
#include "shortcut_keys.h"
#include "command_keys.h"

#include "imgui.h"
#include "test.h"

#include <string.h>

static void check_labels()
{
    char out[64];
    CHECK(strcmp(shortcut_mod_name(1), "Cmd") == 0);
    CHECK(strcmp(shortcut_mod_name(0), "Ctrl") == 0);
    CHECK(shortcut_label(out, sizeof out, 1, "Z") && strcmp(out, "Cmd+Z") == 0);
    CHECK(shortcut_label(out, sizeof out, 1, "Shift+Z") && strcmp(out, "Cmd+Shift+Z") == 0);
    CHECK(shortcut_label(out, sizeof out, 0, "S") && strcmp(out, "Ctrl+S") == 0);
    CHECK(shortcut_label(out, sizeof out, 0, "Shift+Z") && strcmp(out, "Ctrl+Shift+Z") == 0);
    CHECK(!shortcut_label(out, 6, 1, "Shift+Z") && out[0] == '\0');   // no room: nothing half written
    CHECK(shortcut_label(out, sizeof out, SHORTCUT_MAC, "Z") && strcmp(out, SHORTCUT_MAC ? "Cmd+Z" : "Ctrl+Z") == 0);   // this build's own
#ifdef __APPLE__
    CHECK_EQ_INT(SHORTCUT_MAC, 1);
#else
    CHECK_EQ_INT(SHORTCUT_MAC, 0);
#endif
}

// one frame with the keys the SDL backend reports for the pressed modifier (`cmd`: the Mac's Command, SDL's GUI; else Control) and Z; whether
// Shortcut(ImGuiMod_Ctrl | Z) fired
static bool fires(bool mac_behaviour, bool cmd)
{
    ImGuiContext *ctx = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigMacOSXBehaviors = mac_behaviour;
    io.DisplaySize = ImVec2(640, 480);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char *pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    bool fired = false;
    for (int frame = 0; frame < 4; ++frame) {   // a route is taken a frame before it is used, and queued key events go out over frames
        ImGui::NewFrame();
        ImGui::Begin("Car");   // as car_shortcuts: inside a window, every frame
        fired = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) || fired;
        ImGui::End();
        ImGui::EndFrame();
        if (frame == 0) {
            io.AddKeyEvent(cmd ? ImGuiMod_Super : ImGuiMod_Ctrl, true);   // as imgui_impl_sdl3.cpp: SDL_KMOD_GUI is Super, SDL_KMOD_CTRL is Ctrl
            io.AddKeyEvent(cmd ? ImGuiKey_LeftSuper : ImGuiKey_LeftCtrl, true);
            io.AddKeyEvent(ImGuiKey_Z, true);
        }
    }
    ImGui::DestroyContext(ctx);
    return fired;
}

static void check_imgui_takes_the_platform_key()
{
    CHECK(fires(true, true));     // a Mac: Cmd+Z
    CHECK(!fires(true, false));   // a Mac: Ctrl+Z is not the shortcut
    CHECK(fires(false, false));   // elsewhere: Ctrl+Z
    CHECK(!fires(false, true));   // elsewhere: the Windows/Super key is not
    ImGuiContext *ctx = ImGui::CreateContext();   // and a fresh context behaves as this build's menus say
    CHECK_EQ_INT(ImGui::GetIO().ConfigMacOSXBehaviors ? 1 : 0, SHORTCUT_MAC);
    ImGui::DestroyContext(ctx);
}

// one frame with `cmd` (the platform's: Super on a Mac's backend, else Control), Shift when `shift`, and `key` held from the second frame; the command
// command_key saw, NONE for none
static CommandKey command_of(bool cmd, bool shift, ImGuiKey key)
{
    ImGuiContext *ctx = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigMacOSXBehaviors = SHORTCUT_MAC;
    io.DisplaySize = ImVec2(640, 480);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char *pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    io.IniFilename = nullptr;   // no imgui.ini left where the test runs
    CommandKey seen = COMMAND_NONE;
    for (int frame = 0; frame < 4; ++frame) {
        ImGui::NewFrame();
        ImGui::Begin("Track");
        const CommandKey now = command_key(false);
        if (now != COMMAND_NONE) seen = now;
        ImGui::End();
        ImGui::EndFrame();
        if (frame == 0) {
            if (cmd) {
                io.AddKeyEvent(SHORTCUT_MAC ? ImGuiMod_Super : ImGuiMod_Ctrl, true);
                io.AddKeyEvent(SHORTCUT_MAC ? ImGuiKey_LeftSuper : ImGuiKey_LeftCtrl, true);
            }
            if (shift) io.AddKeyEvent(ImGuiMod_Shift, true), io.AddKeyEvent(ImGuiKey_LeftShift, true);
            io.AddKeyEvent(key, true);
        }
    }
    ImGui::DestroyContext(ctx);
    return seen;
}

// Save, Undo and Redo of both editors: on the command key, never a plain letter (a track's Save was S alone; owner 2026-11-20)
static void check_command_keys()
{
    CHECK_EQ_INT(command_of(true, false, ImGuiKey_S), COMMAND_SAVE);
    CHECK_EQ_INT(command_of(false, false, ImGuiKey_S), COMMAND_NONE);   // S alone: typing an S saves nothing
    CHECK_EQ_INT(command_of(true, false, ImGuiKey_Z), COMMAND_UNDO);
    CHECK_EQ_INT(command_of(true, true, ImGuiKey_Z), COMMAND_REDO);
    CHECK_EQ_INT(command_of(true, false, ImGuiKey_Y), COMMAND_REDO);
    CHECK_EQ_INT(command_of(false, false, ImGuiKey_Z), COMMAND_NONE);
}

int main()
{
    check_labels();
    check_imgui_takes_the_platform_key();
    check_command_keys();
    return TEST_REPORT();
}
