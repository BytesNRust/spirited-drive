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

void ui_event(const SDL_Event *ev)
{
    ImGui_ImplSDL3_ProcessEvent(ev);
}

namespace {

// the layout of a first run (no ini): the top view in the middle, the limits under it, the project and the points to its right
void default_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    ImGuiID right = 0, centre = dock, bottom = 0, points = 0;
    right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.27f, nullptr, &centre);
    bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.30f, nullptr, &centre);
    points = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.55f, nullptr, &right);
    ImGui::DockBuilderDockWindow("Viewport", centre);
    ImGui::DockBuilderDockWindow(PANEL_LIMITS, bottom);
    ImGui::DockBuilderDockWindow(PANEL_PROJECT, right);
    ImGui::DockBuilderDockWindow(PANEL_POINTS, points);
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

}   // namespace

void ui_frame(SDL_Renderer *ren, App *a)
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    const ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    static bool laid_out = false;
    if (!laid_out) {
        laid_out = true;
        if (!g_had_layout) default_layout(dock);
    }
    shortcuts(a);
    viewport_panel(a);
    project_panel(a);
    points_panel(a);
    limits_panel(a);
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ren);
}
