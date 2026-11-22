// The Edit menu's Cut, Copy, Paste and Select all (editor/edit_clipboard.cpp, owner item 3): a click away from a text field takes the keyboard from it (as a click on
// the menu does), and the item still acts on that field's own selection: its text is copied, cut or replaced as Cmd+C, X, V (Ctrl elsewhere) would. No window: a context
// fed the events the SDL backend sends, and a clipboard of its own.
#include "edit_clipboard.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "test.h"

#include <string.h>
#include <string>

static std::string clipboard;
static char text[64];

static const char *get_clipboard(ImGuiContext *) { return clipboard.c_str(); }
static void set_clipboard(ImGuiContext *, const char *s) { clipboard = s; }

static void frame()
{
    ImGui::NewFrame();
    edit_clipboard_frame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(400, 120));
    ImGui::Begin("W", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::InputText("##f", text, sizeof text);
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(0, 200));   // the menu bar's place: a click on it is how an Edit item is reached
    ImGui::SetNextWindowSize(ImVec2(400, 200));
    ImGui::Begin("M", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar);
    ImGui::End();
    ImGui::EndFrame();
}

static void frames(int n)
{
    for (int i = 0; i < n; ++i) frame();
}

static void click(ImGuiIO &io, float x, float y)
{
    io.AddMousePosEvent(x, y);
    frames(2);
    io.AddMouseButtonEvent(0, true);
    frames(2);
    io.AddMouseButtonEvent(0, false);
    frames(2);
}

static ImGuiID field_id()
{
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(400, 120));
    ImGui::Begin("W", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    const ImGuiID id = ImGui::GetID("##f");
    ImGui::End();
    ImGui::EndFrame();
    return id;
}

// an item: the text field's selection as it was left, a click away, the action, and the frames it takes
static void act(ImGuiIO &io, EditAction action)
{
    click(io, 300, 300);   // the menu: a click that is not on the field
    CHECK(ImGui::GetActiveID() == 0);
    CHECK(edit_clipboard_ready());
    edit_clipboard_do(action);
    for (int i = 0; i < 20 && edit_clipboard_busy(); ++i) frame();
    CHECK(!edit_clipboard_busy());
    frames(3);
}

static void run(bool mac_behaviour)
{
    ImGuiContext *ctx = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigMacOSXBehaviors = mac_behaviour;
    io.DisplaySize = ImVec2(640, 480);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::GetPlatformIO().Platform_GetClipboardTextFn = get_clipboard;
    ImGui::GetPlatformIO().Platform_SetClipboardTextFn = set_clipboard;
    unsigned char *pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    edit_clipboard_reset();
    clipboard.clear();
    memset(text, 0, sizeof text);

    CHECK(!edit_clipboard_ready());
    edit_clipboard_do(EDIT_COPY);   // no field yet: nothing is queued
    CHECK(!edit_clipboard_busy());
    frames(2);
    const ImGuiID id = field_id();
    click(io, 40, 16);   // into the field
    CHECK(ImGui::GetActiveID() == id);
    io.AddInputCharactersUTF8("hello world");
    frames(3);
    CHECK(strcmp(text, "hello world") == 0);
    ImGui::GetInputTextState(id)->SetSelection(0, 5);   // "hello"
    frames(2);

    act(io, EDIT_COPY);
    CHECK(clipboard == "hello" && strcmp(text, "hello world") == 0);

    act(io, EDIT_CUT);   // the selection is the one it was left with: still "hello"
    CHECK(clipboard == "hello" && strcmp(text, " world") == 0);

    clipboard = "XY";
    act(io, EDIT_PASTE);   // at the place the cut left: the start
    CHECK(strcmp(text, "XY world") == 0);

    act(io, EDIT_SELECT_ALL);
    act(io, EDIT_COPY);
    CHECK(clipboard == "XY world");

    click(io, 40, 16);   // back into the field, then a click on nothing: the field is left, the items are not for it any more
    CHECK(ImGui::GetActiveID() == id && edit_clipboard_ready());
    click(io, 600, 450);
    frames(2);
    CHECK(!edit_clipboard_ready());
    click(io, 40, 16);   // a click on the field again: it is the one again
    frames(2);
    click(io, 300, 300);   // on the menus: it stays
    CHECK(edit_clipboard_ready());
    ImGui::DestroyContext(ctx);
}

int main()
{
    run(true);    // a Mac: Cmd
    run(false);   // elsewhere: Ctrl
    return TEST_REPORT();
}
