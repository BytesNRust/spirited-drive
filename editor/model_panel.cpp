// The Model panel (dep59 car editor, step 2): the open car in 3D as the game draws it. The classic rasterizer of the core (raster/car_scene.h) draws the high
// model into an 8-bit surface, turned and placed by an orbit camera (raster/orbit.h), and the surface goes to the window as a texture. Drag with the left button
// to turn the car all the way round, on every axis, the wheel zooms, F / S / R / T are the front, side, rear and top, Home the first view; Auto-rotate
// turns it by itself. Painted as the Preview window paints it (car_preview), in its Day / Night and lamps.
#include "panels.h"

#include "imgui.h"

#include <SDL3/SDL.h>

extern "C" {
#include "formats/police_flash.h"
#include "model_scene.h"
}
#include "car_export.h"
#include "car_race.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const PANEL_MODEL = "Model";

namespace {

enum { MAX_W = 2048, AUTO_DEG_PER_SECOND = 24 };

struct Model {
    char loaded[APP_PATH] = "";   // the CFM the scene holds (or tried to)
    ModelScene *scene = nullptr;
    char why[200] = "";
    char key_dir[APP_PATH] = "";   // what the picture was made from: the project, its masks, the paint (the Preview window's own choice)
    unsigned key_generation = 0, key_version = 0;
    int key_paint = -2;
    uint8_t key_rgb[3] = {};
    bool painted = false;
    Orbit orbit = {};
    bool auto_rotate = false, dirty = true;
    bool shine = true;            // the body shine, on: the owner checks the shine and matte masks with it
    bool shines = false;          // the car has texels the shine reaches
    char envmap[APP_PATH] = "";   // the kit's own car_envmap.png, when it has one
    int preset = 0;               // an orbit asked for on the command line, waiting for the car
    double preset_yaw = 0, preset_pitch = 0;
    int32_t preset_dist = 0;
    NightSettings key_night = {};   // the Preview's Day / Night and lamps the picture was drawn in, and the police flash's phase
    int key_phase = -1;
    SDL_Texture *tex = nullptr;
    int tw = 0, th = 0;
    uint8_t *rgba = nullptr;
    size_t capacity = 0;
};

Model g;

// the shine on or off as the box says, with the kit's envmap or the game's own
void set_shine()
{
    g.shines = g.scene != nullptr && model_scene_shine(g.scene, g.shine, g.envmap[0] != '\0' ? g.envmap : nullptr);
}

void close_scene()
{
    model_scene_close(g.scene);
    g.scene = nullptr;
}

// the scene of the car as the race paints it (the Preview window's paint); the car file as it is when it cannot be painted
void open_scene(App *a, const uint8_t *rgb)
{
    close_scene();
    g.why[0] = 0;
    const ExportCar *oc = car_race_original_of(&a->car);
    char masks[APP_PATH], fallback[APP_PATH], why[sizeof g.why];
    snprintf(masks, sizeof masks, "%s/car_masks/%s", a->car.piece, a->car.folder);
    snprintf(fallback, sizeof fallback, "%s/car_masks/%s", a->export_src.data, oc != nullptr ? oc->folder : "");
    PreviewIn in = {};
    in.cfm = a->car.cfm;
    in.stem = oc != nullptr ? oc->folder : a->car.folder;
    in.masks_dir = a->car.has_masks_dir ? masks : nullptr;
    in.fallback_dir = oc != nullptr && a->export_src.data[0] != '\0' ? fallback : nullptr;
    in.edit = a->mask.sel >= 0 && app_mask_modified(a) ? &a->mask.me : nullptr;
    in.paint = rgb;
    in.added = oc == nullptr;
    g.scene = model_scene_open_painted(&in, why, sizeof why);
    g.painted = g.scene != nullptr;
    if (g.scene == nullptr) g.scene = model_scene_open(a->car.cfm, g.why, sizeof g.why);   // the file as it is: masks that are not there yet
    snprintf(g.envmap, sizeof g.envmap, "%s/car_envmap.png", a->car.piece);   // a mod's car_envmap.png replaces the game's, as in the race
    FILE *f = fopen(g.envmap, "rb");
    if (f != nullptr) fclose(f);
    else g.envmap[0] = '\0';
    set_shine();
    g.dirty = true;
}

// the keys the picture is made from
bool stale(const App *a, const uint8_t *rgb)
{
    return strcmp(g.key_dir, a->dir) != 0 || g.key_generation != a->mask.generation || g.key_version != (a->mask.sel >= 0 ? a->mask.me.version : 0u)
           || g.key_paint != a->preview_paint || (rgb != nullptr && memcmp(g.key_rgb, rgb, 3) != 0);
}

void remember(const App *a, const uint8_t *rgb)
{
    snprintf(g.key_dir, sizeof g.key_dir, "%s", a->dir);
    g.key_generation = a->mask.generation;
    g.key_version = a->mask.sel >= 0 ? a->mask.me.version : 0u;
    g.key_paint = a->preview_paint;
    if (rgb != nullptr) memcpy(g.key_rgb, rgb, 3);
}

// the car file read and the scene opened on it; the camera starts at home (or at the command line's orbit)
void load(App *a, const uint8_t *rgb)
{
    snprintf(g.loaded, sizeof g.loaded, "%s", a->car.cfm);
    open_scene(a, rgb);
    remember(a, rgb);
    if (g.scene == nullptr) return;
    orbit_init(&g.orbit, model_scene_radius(g.scene));
    if (g.preset) {
        orbit_set_degrees(&g.orbit, g.preset_yaw, g.preset_pitch);
        if (g.preset_dist > 0) orbit_set_dist(&g.orbit, g.preset_dist);
        g.preset = 0;
    }
}

// the surface and the texture, for a picture of w x h
bool make_target(SDL_Renderer *ren, int w, int h)
{
    if (g.tex != nullptr && g.tw == w && g.th == h) return true;
    if (g.tex != nullptr) SDL_DestroyTexture(g.tex), g.tex = nullptr;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (n > g.capacity) {
        free(g.rgba);
        g.rgba = static_cast<uint8_t *>(malloc(4 * n));
        g.capacity = g.rgba != nullptr ? n : 0;
        if (g.capacity == 0) return false;
    }
    g.tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (g.tex == nullptr) return false;
    SDL_SetTextureBlendMode(g.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g.tex, SDL_SCALEMODE_NEAREST);
    g.tw = w, g.th = h;
    g.dirty = true;
    return true;
}

// the picture in the Preview window's light: Day or Night, the track the night is of, the light at the car, the lamps (editor/night_view.h)
void draw_picture(const App *a, int phase, int w, int h)
{
    const CarNightState night = night_settings_state(&a->night, phase * POLICE_FLASH_TICKS);
    if (model_scene_render(g.scene, &g.orbit, w, h, &night, g.rgba)) SDL_UpdateTexture(g.tex, nullptr, g.rgba, w * 4);
    g.key_night = a->night, g.key_phase = phase;
    g.dirty = false;
}

// the picture's size: the whole area, scaled down to what the rasterizer takes (at most MAX_W x its rows) with the area's shape kept; 0 when too small
bool picture_size(int area_w, int area_h, int *w, int *h)
{
    if (area_w < 64 || area_h < 64) return false;
    double scale = 1.0;
    if (area_w > MAX_W) scale = static_cast<double>(MAX_W) / area_w;
    if (area_h * scale > model_scene_max_rows()) scale = static_cast<double>(model_scene_max_rows()) / area_h;
    *w = static_cast<int>(area_w * scale), *h = static_cast<int>(area_h * scale);
    return *w >= 2 && *h >= 2;
}

void keys()
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || io.KeyCtrl || io.KeyAlt || io.KeySuper) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) return;
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) orbit_view(&g.orbit, ORBIT_FRONT), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) orbit_view(&g.orbit, ORBIT_SIDE), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) orbit_view(&g.orbit, ORBIT_REAR), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_T, false)) orbit_view(&g.orbit, ORBIT_TOP), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) orbit_home(&g.orbit), g.dirty = true;
}

void toolbar()
{
    if (ImGui::Button("Front")) orbit_view(&g.orbit, ORBIT_FRONT), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Side")) orbit_view(&g.orbit, ORBIT_SIDE), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Rear")) orbit_view(&g.orbit, ORBIT_REAR), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Top")) orbit_view(&g.orbit, ORBIT_TOP), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Home")) orbit_home(&g.orbit), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-rotate", &g.auto_rotate)) g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Shine", &g.shine)) set_shine(), g.dirty = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The body's shine as the race draws it, where the masks say paint or shine (matte texels stay as they are).\n"
                          "Its tones are taken from the car's own colours; in a race, from the screen's.%s",
                          g.envmap[0] != '\0' ? "\nThe kit's car_envmap.png is the sky it reflects." : "");
    ImGui::SameLine();
    if (ImGui::Button("Reload")) g.loaded[0] = 0;   // the file again: after Rehacer or a new bank
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Read the car file again");
}

void turn_and_zoom()
{
    ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        orbit_drag(&g.orbit, -static_cast<int>(roundf(io.MouseDelta.x)), static_cast<int>(roundf(io.MouseDelta.y)));   // the car follows the pointer
        g.dirty = true;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0) orbit_zoom(&g.orbit, io.MouseWheel > 0 ? 1 : -1), g.dirty = true;
}

}   // namespace

void model_panel_shine(bool on)
{
    g.shine = on;
    set_shine();
    g.dirty = true;
}

void model_panel_preset(double yaw, double pitch, int dist)
{
    g.preset = 1, g.preset_yaw = yaw, g.preset_pitch = pitch, g.preset_dist = dist;
}

void model_panel(App *a, SDL_Renderer *ren)
{
    if (ImGui::Begin(PANEL_MODEL)) {
        const int paint = a->preview_paint;
        const ExportCar *oc = car_race_original_of(&a->car);   // the paints as the Preview window has them: a masks project and the game's traffic have none
        const int count = a->car.masks_only || (oc != nullptr && oc->kind != EXPORT_DRIVABLE) ? 0 : a->edit.now.paint_count;
        const uint8_t *rgb = paint >= 0 && paint < count ? a->edit.now.paints[paint].rgb : nullptr;
        const bool busy = a->mask.me.stroke != 0 || app_rehacer_busy(a) || a->car.cfm[0] == '\0';
        if (!busy && strcmp(g.loaded, a->car.cfm) != 0) load(a, rgb);
        else if (!busy && g.scene != nullptr && stale(a, rgb)) {   // a mask edited or saved, another paint: the same camera on the car made again
            open_scene(a, rgb);
            remember(a, rgb);
        }
        if (g.scene == nullptr) {
            ImGui::TextWrapped("%s", g.why[0] != 0 ? g.why : "No car is open.");
        } else {
            toolbar();
            keys();
            const ImVec2 size = ImGui::GetContentRegionAvail();
            const int lines = g.shine ? 2 : 1;   // the words under the picture
            const int area_w = static_cast<int>(size.x), area_h = static_cast<int>(size.y) - lines * static_cast<int>(ImGui::GetTextLineHeightWithSpacing());
            int w = 0, h = 0;   // the picture fills the area (car_scene keeps its pixels square, the car whole in the smaller side)
            if (picture_size(area_w, area_h, &w, &h) && make_target(ren, w, h)) {
                if (g.auto_rotate) {
                    const int step = static_cast<int>(roundf(ImGui::GetIO().DeltaTime * AUTO_DEG_PER_SECOND * 16.0f));
                    orbit_drag(&g.orbit, -step / ORBIT_DRAG - (step % ORBIT_DRAG != 0), 0);   // at least a step a frame: the yaw moves by 1/16 degrees
                    g.dirty = true;
                }
                const int phase = a->night.police ? static_cast<int>((SDL_GetTicks() / 500u) & 1u) : 0;   // the flash, as the Preview window has it
                if (g.dirty || g.key_phase != phase || memcmp(&g.key_night, &a->night, sizeof a->night) != 0) draw_picture(a, phase, w, h);
                const ImVec2 at = ImGui::GetCursorScreenPos(), area(static_cast<float>(area_w), static_cast<float>(area_h));
                ImGui::InvisibleButton("##orbit", area);
                turn_and_zoom();
                ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(g.tex), at, ImVec2(at.x + area.x, at.y + area.y));
                // the Preview's words: a paint chosen is the race's; none (or a car the paint cannot be made for) is the base texture
                ImGui::TextDisabled("Drag to turn, wheel to zoom, F S R T Home. %s", g.painted && rgb != nullptr ? "As the race paints it." : "Base texture (no colour).");
                if (g.shine)
                    ImGui::TextDisabled("%s", g.shines ? "Shine: its tones from the car's own colours (a race takes them from the screen's)."
                                                       : "Shine: no texel of this car shines (its masks mark none as paint or shine).");
            }
        }
    }
    ImGui::End();
}
