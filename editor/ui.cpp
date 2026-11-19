// The editor's interface layer (ui.h): Dear ImGui with docking over the SDL renderer: the dock space, the shortcuts and the panels (panels.cpp);
// the top view is viewport.cpp's.
#include "ui.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

extern "C" {
#include "formats/track_gen.h"
#include "interact.h"
}
#include "panels.h"
#include "viewport.h"

#include <stdio.h>

static char g_ini[APP_PATH];
static bool g_had_layout;   // the ini file was there at the start: its layout is the user's, not ours to replace

int ui_open(SDL_Window *win, SDL_Renderer *ren, const char *ini, char *why, size_t why_cap)
{
    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr) {
        snprintf(why, why_cap, "ImGui: no context");
        return 0;
    }
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    if (ini != nullptr) {
        snprintf(g_ini, sizeof g_ini, "%s", ini);
        io.IniFilename = g_ini;
        FILE *f = fopen(g_ini, "rb");
        g_had_layout = f != nullptr;
        if (f != nullptr) fclose(f);
    } else {
        io.IniFilename = nullptr;
    }
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForSDLRenderer(win, ren)) {
        snprintf(why, why_cap, "ImGui: the SDL backend did not start");
        ImGui::DestroyContext();
        return 0;
    }
    if (!ImGui_ImplSDLRenderer3_Init(ren)) {
        snprintf(why, why_cap, "ImGui: the renderer backend did not start");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return 0;
    }
    return 1;
}

void ui_close(void)
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

int ui_popup_open(void)
{
    return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) ? 1 : 0;
}

void ui_event(const SDL_Event *ev)
{
    ImGui_ImplSDL3_ProcessEvent(ev);
}

namespace {

// the layout of a first run (no ini): the top view in the middle, the limits and the problems (two tabs) under it, the project and the points to its right
void default_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    ImGuiID right = 0, centre = dock, bottom = 0, points = 0, node = 0;
    right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.33f, nullptr, &centre);
    bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.30f, nullptr, &centre);
    points = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.55f, nullptr, &right);
    ImGui::DockBuilderDockWindow("Viewport", centre);
    ImGui::DockBuilderDockWindow(PANEL_LIMITS, bottom);
    ImGui::DockBuilderDockWindow(PANEL_PROBLEMS, bottom);   // its own tab beside Limits
    const ImGuiID track = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.34f, nullptr, &right);   // the road's width above the project
    ImGui::DockBuilderDockWindow(PANEL_TRACK, track);
    ImGui::DockBuilderDockWindow(PANEL_PROJECT, right);
    node = ImGui::DockBuilderSplitNode(points, ImGuiDir_Down, 0.45f, nullptr, &points);
    ImGui::DockBuilderDockWindow(PANEL_POINTS, points);
    ImGui::DockBuilderDockWindow(PANEL_NODE, node);
    ImGui::DockBuilderFinish(dock);
}

// the shortcuts, when no text field has the keyboard
void shortcuts(App *a)
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || io.KeyCtrl || io.KeyAlt || io.KeySuper) return;   // Cmd+S and the like are not ours
    const double cx = a->view.width / 2.0, cy = a->view.height / 2.0;
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) app_add_point(a);
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) app_insert_point(a);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) app_remove_point(a);
    else if (ImGui::IsKeyPressed(ImGuiKey_X, false)) app_remove_point(a);
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) app_save(a);
    if (ImGui::IsKeyPressed(ImGuiKey_B, false)) app_build(a);
    if (ImGui::IsKeyPressed(ImGuiKey_U, false)) app_install(a);
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) interact_fit(a);
    if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) interact_zoom(a, cx, cy, VIEW_ZOOM_STEP);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) interact_zoom(a, cx, cy, 1 / VIEW_ZOOM_STEP);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) interact_nudge(a, -INTERACT_NUDGE, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) interact_nudge(a, INTERACT_NUDGE, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) interact_nudge(a, 0, INTERACT_NUDGE);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) interact_nudge(a, 0, -INTERACT_NUDGE);
}

// the File menu: a new project, an open one, the recent ones, Save, Quit; each asks the app (app_ask), which asks about unsaved changes first
void main_menu(App *a)
{
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New project...")) app_ask(a, APP_ACT_NEW, "");
        if (ImGui::MenuItem("Open project...")) app_ask(a, APP_ACT_OPEN, "");
        if (ImGui::BeginMenu("Open recent", a->recent.count > 0)) {
            for (int i = 0; i < a->recent.count; ++i) {
                ImGui::PushID(i);
                if (ImGui::MenuItem(a->recent.dir[i], nullptr, false, recent_is_project(a->recent.dir[i]))) app_ask(a, APP_ACT_RECENT, a->recent.dir[i]);
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "S")) app_save(a);
        if (ImGui::MenuItem("Quit")) app_ask(a, APP_ACT_QUIT, "");
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

// "Save changes to <project> before ...?": Save, Discard, Cancel (Escape, in main.c, is Cancel; Enter is Save, once the question has been on screen a
// frame, so the Enter that chose a menu item cannot answer it)
void confirm_popup(App *a)
{
    static const char *const NAME = "Unsaved changes";
    static int age = 0;               // frames the question has been up
    static bool save_failed = false;  // the last Save did not write: the question stays, with the reason
    if (a->confirm == APP_ACT_NONE) {
        age = 0;
        save_failed = false;
    } else if (!ImGui::IsPopupOpen(NAME)) {
        ImGui::OpenPopup(NAME);
    }
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (a->confirm == APP_ACT_NONE) {   // answered elsewhere (Escape)
        ImGui::CloseCurrentPopup();
    } else {
        ++age;
        const char *then = a->confirm == APP_ACT_QUIT ? "quitting" : a->confirm == APP_ACT_NEW || a->confirm == APP_ACT_NEW_AT ? "starting a new project"
                                                                                                                              : "opening another project";
        ImGui::Text("Save the changes to \"%s\" before %s?", a->project.id, then);
        ImGui::Spacing();
        int answer = -1;
        if (ImGui::Button("Save")) answer = APP_ANSWER_SAVE;
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Discard")) answer = APP_ANSWER_DISCARD;
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) answer = APP_ANSWER_CANCEL;
        if (age > 1 && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) answer = APP_ANSWER_SAVE;
        if (answer >= 0) {
            app_answer(a, answer);
            save_failed = answer == APP_ANSWER_SAVE && a->confirm != APP_ACT_NONE;
            if (a->confirm == APP_ACT_NONE) ImGui::CloseCurrentPopup();
        }
        if (save_failed) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", a->status);
    }
    ImGui::EndPopup();
}

}   // namespace

void ui_frame(SDL_Renderer *ren, App *a)
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    main_menu(a);
    const ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    static bool laid_out = false;
    if (!laid_out) {
        laid_out = true;
        if (!g_had_layout) default_layout(dock);
    }
    if (a->confirm == APP_ACT_NONE) shortcuts(a);   // the keys edit nothing while the question is up
    viewport_panel(a);
    project_panel(a);
    track_panel(a);
    points_panel(a);
    limits_panel(a);
    problems_panel(a);
    node_panel(a);
    confirm_popup(a);
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ren);
}
