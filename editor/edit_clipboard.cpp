#include "edit_clipboard.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace {

enum { GIVE_UP_FRAMES = 6 };

struct Last {
    ImGuiID id = 0;
    int select_start = 0, select_end = 0;
} last;

struct Pending {
    bool on = false;
    EditAction action = EDIT_COPY;
    int stage = 0;    // 0 the field is to be activated, 1 waiting for it to be active, 2 the chord is down
    int waited = 0;
} pending;

ImGuiKey key_of(EditAction a)
{
    return a == EDIT_CUT ? ImGuiKey_X : a == EDIT_COPY ? ImGuiKey_C : a == EDIT_PASTE ? ImGuiKey_V : ImGuiKey_A;
}

// the key the shortcuts hold: Cmd on a Mac (the backend's Cmd arrives as Super and AddKeyEvent turns it into Ctrl there), Ctrl elsewhere
ImGuiKey shortcut_mod()
{
    return ImGui::GetIO().ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
}

void remember(ImGuiContext &g)
{
    ImGuiInputTextState *st = ImGui::GetInputTextState(g.ActiveId);
    if (st == nullptr) return;
    last.id = g.ActiveId;
    last.select_start = st->GetSelectionStart(), last.select_end = st->GetSelectionEnd();   // a cursor with no selection has both at its place
}

void step(ImGuiContext &g)
{
    ImGuiIO &io = ImGui::GetIO();
    if (pending.stage == 0) {   // give the field back: the same as a click on it that does not move the text's cursor
        g.NavNextActivateId = last.id;
        g.NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        pending.stage = 1;
        return;
    }
    if (pending.stage == 1) {
        ImGuiInputTextState *st = ImGui::GetInputTextState(last.id);
        if (st == nullptr || g.ActiveId != last.id) {
            if (++pending.waited > GIVE_UP_FRAMES) pending = Pending();   // the field is not there any more
            return;
        }
        st->SetSelection(last.select_start, last.select_end);
        io.AddKeyEvent(shortcut_mod(), true);
        io.AddKeyEvent(key_of(pending.action), true);
        pending.stage = 2;
        return;
    }
    io.AddKeyEvent(key_of(pending.action), false);   // the chord is let go the frame after it was read
    io.AddKeyEvent(shortcut_mod(), false);
    pending = Pending();
}

}   // namespace

void edit_clipboard_frame(void)
{
    ImGuiContext &g = *ImGui::GetCurrentContext();
    if (!pending.on || pending.stage == 0) remember(g);   // while the action runs the field is the one it gave back: nothing new to remember
    if (pending.on) step(g);
}

int edit_clipboard_ready(void)
{
    return last.id != 0;
}

void edit_clipboard_do(EditAction action)
{
    if (last.id == 0 || pending.on) return;
    pending = Pending();
    pending.on = true;
    pending.action = action;
}

int edit_clipboard_busy(void)
{
    return pending.on;
}

void edit_clipboard_reset(void)
{
    last = Last();
    pending = Pending();
}
