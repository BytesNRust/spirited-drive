// The editor's interface layer (ui.h): Dear ImGui with docking over the SDL renderer. Phase 1 of the move to ImGui: an empty dock space
// and one panel with what the app says; the top view is still draw.c's and the panels come in the next branches.
#include "ui.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <stdio.h>

static char g_ini[APP_PATH];

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

int ui_event(const SDL_Event *ev)
{
    ImGui_ImplSDL3_ProcessEvent(ev);
    const ImGuiIO &io = ImGui::GetIO();
    switch (ev->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_WHEEL: return io.WantCaptureMouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (ev->key.key == SDLK_ESCAPE) return 0;   // Escape always reaches the editor: the way out
        return io.WantCaptureKeyboard || io.WantTextInput;
    case SDL_EVENT_TEXT_INPUT: return io.WantTextInput;
    default: return 0;
    }
}

void ui_frame(SDL_Renderer *ren, const App *a)
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    if (ImGui::Begin("Project")) {
        ImGui::TextUnformatted(a->dir);
        ImGui::Text("%d control points, %d nodes", a->trace.count, a->node_count);
        ImGui::TextWrapped("%s", a->status);
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ren);
}
