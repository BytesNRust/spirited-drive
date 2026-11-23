// The Model panel (dep59 car editor, step 2): the open car in 3D as the game draws it. The classic rasterizer of the core (raster/car_scene.h) draws the high
// model into an 8-bit surface, turned and placed by an orbit camera (raster/orbit.h), and the surface goes to the window as a texture. Drag with the left button
// to turn the car all the way round, on every axis, the wheel zooms, F / S / R / T are the front, side, rear and top, Home the first view; Auto-rotate
// turns it by itself. Painted as the Preview window paints it (car_preview), in its Day / Night and lamps.
#include "panels.h"
#include "help_marker.h"
#include "model_view.h"
#include "ui_probe.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <SDL3/SDL.h>

extern "C" {
#include "formats/police_flash.h"
#include "model_scene.h"
#include "card_backdrop.h"
#include "card_central.h"
#include "card_photo.h"
}
#include "car_export.h"
#include "car_race.h"
extern "C" void shine_draw_set_soft(int on);
extern "C" void shine_draw_set_render_scale(int scale);   // core/raster/shine_draw.h, not included: its View clashes with the editor's

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const PANEL_MODEL = "Model";

namespace {

enum { MAX_W = 2048, FRAME_PERCENT = 80, AUTO_DEG_PER_SECOND = 24, GAME_TICKS_PER_SECOND = 30 /* the race calls its wheels 30 times a second (game/race/race_sim.h; the Studio links only the core) */ };

struct Model {
    char loaded[APP_PATH] = "";   // the CFM the scene holds (or tried to)
    unsigned loaded_serial = 0;   // the car file as a->cdisk.serial counted it then: another program wrote the file since when it moved
    ModelScene *scene = nullptr;
    char why[200] = "";
    char key_dir[APP_PATH] = "";   // what the picture was made from: the project, its masks, the paint (the Preview window's own choice)
    unsigned key_generation = 0;
    int key_paint = -2;
    uint8_t key_rgb[3] = {};
    bool painted = false;
    Orbit orbit = {};
    bool auto_rotate = false, dirty = true;
    CardBackdrop backdrop = card_backdrop_default();   // what the card photo stands in front of (session only)
    uint8_t back_rgba[CARD_PHOTO_W * CARD_PHOTO_H * 4] = {};   // the backdrop made, and the reason when its picture cannot be
    bool back_dirty = true;
    char back_why[400] = "";
    bool card_view = false;       // the picture is the card's: 309 x 166 over the card's backdrop, as card.png would be saved
    bool as_card = false;         // and put in Control Central's palette as the game draws it (card_central.h)
    uint8_t cpal[768] = {};       // Central's palette, read from the game's files (cpal_cd says where from)
    bool cpal_ok = false;
    char cpal_cd[APP_PATH] = "", cpal_why[240] = "";
    int big_ask = 0;              // 1: float the window large on the next frame, -1: dock it back where it was (Enlarge / Dock back)
    bool big = false;             // the window is floating, enlarged
    ImGuiID dock_id = 0;          // where it was docked, to go back to
    bool docked = false;          // the window was docked when last drawn (the flow test reads it)
    ImGuiID parent_id = 0;        // the node that held it and its neighbours: when it leaves (Enlarge) its own node is gone, this one stays and is split to put it back
    ImGuiDir split_dir = ImGuiDir_Up; // the side of that node the window was on
    float split_ratio = 0.4f;     // and its share of it
    int render_scale = 0;         // the game's render_scale for this window (1..3): 0 until the player's own is read; changing it never writes their options (model_view_render_size)
    int soft = -1;                // the shine's soft mask edges: -1 until the player's own choice is read (shine_soft), then 0 or 1
    bool shine = true;            // the body shine, on: the owner checks the shine and matte masks with it
    bool shines = false;          // the car has texels the shine reaches
    bool shine_asked = false;     // the box (or the command line) changed it: set when the window is next drawn
    char envmap[APP_PATH] = "";   // the kit's own car_envmap.png, when it has one
    int preset = 0;               // an orbit asked for on the command line, waiting for the car
    double preset_yaw = 0, preset_pitch = 0;
    int32_t preset_dist = 0;
    bool fit = false;             // the car is to be framed (made to fill FRAME_PERCENT of the picture) when the picture's size is known: on open, on another car, on Home
    int key_wheel = -1;           // the wheel frame the picture was drawn with
    NightSettings key_night = {};   // the Preview's Day / Night and lamps the picture was drawn in, and the police flash's phase
    int key_phase = -1;
    SDL_Texture *tex = nullptr;
    int tw = 0, th = 0;
    uint8_t *rgba = nullptr;
    size_t capacity = 0;
};

Model g;

// what the car is painted from (the Preview window's), with the folders' names it points to
struct Source {
    PreviewIn in = {};
    char masks[APP_PATH], fallback[APP_PATH];
};

void source_of(App *a, const uint8_t *rgb, Source *s)
{
    const ExportCar *oc = car_race_original_of(&a->car);
    car_project_masks_dir(&a->car, s->masks, sizeof s->masks);
    snprintf(s->fallback, sizeof s->fallback, "%s/car_masks/%s", a->export_src.data, oc != nullptr ? oc->folder : "");
    s->in.cfm = a->car.cfm;
    s->in.stem = oc != nullptr ? oc->folder : a->car.folder;
    s->in.masks_dir = a->car.has_masks_dir ? s->masks : nullptr;
    s->in.fallback_dir = oc != nullptr && a->export_src.data[0] != '\0' ? s->fallback : nullptr;
    s->in.paint = rgb;
    s->in.added = oc == nullptr;
}

// the shine on or off as the box says, with the kit's envmap or the game's own; the masks' draft read the first time it is on
void set_shine(App *a, const uint8_t *rgb)
{
    Source src;
    source_of(a, rgb, &src);
    g.shines = g.scene != nullptr && model_scene_shine(g.scene, g.shine, &src.in, g.envmap[0] != '\0' ? g.envmap : nullptr);
    g.shine_asked = false;
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
    char why[sizeof g.why];
    Source src;
    source_of(a, rgb, &src);
    g.scene = model_scene_open_painted(&src.in, why, sizeof why);
    g.painted = g.scene != nullptr;
    if (g.scene == nullptr) g.scene = model_scene_open(a->car.cfm, g.why, sizeof g.why);   // the file as it is: masks that are not there yet
    snprintf(g.envmap, sizeof g.envmap, "%s/car_envmap.png", a->car.root);   // a mod's car_envmap.png replaces the game's, as in the race
    FILE *f = fopen(g.envmap, "rb");
    if (f != nullptr) fclose(f);
    else g.envmap[0] = '\0';
    set_shine(a, rgb);
    g.dirty = true;
}

// the keys the picture is made from
bool stale(const App *a, const uint8_t *rgb)
{
    return strcmp(g.key_dir, a->dir) != 0 || g.key_generation != a->mask.generation
           || g.key_paint != a->preview_paint || (rgb != nullptr && memcmp(g.key_rgb, rgb, 3) != 0);
}

void remember(const App *a, const uint8_t *rgb)
{
    snprintf(g.key_dir, sizeof g.key_dir, "%s", a->dir);
    g.key_generation = a->mask.generation;
    g.key_paint = a->preview_paint;
    if (rgb != nullptr) memcpy(g.key_rgb, rgb, 3);
}

// the car file read and the scene opened on it; the camera starts at home (or at the command line's orbit)
void load(App *a, const uint8_t *rgb)
{
    const bool again = g.scene != nullptr && strcmp(g.loaded, a->car.cfm) == 0;   // the same car read again (Convert PSD, Build, a file changed on disk): the camera stays
    snprintf(g.loaded, sizeof g.loaded, "%s", a->car.cfm);
    g.loaded_serial = a->cdisk.serial;
    open_scene(a, rgb);
    remember(a, rgb);
    if (g.scene == nullptr) return;
    if (!again) orbit_init(&g.orbit, model_scene_radius(g.scene)), g.fit = true;
    if (g.preset) {
        orbit_set_degrees(&g.orbit, g.preset_yaw, g.preset_pitch);
        if (g.preset_dist > 0) orbit_set_dist(&g.orbit, g.preset_dist), g.fit = false;   // a distance asked for is kept
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

// the backdrop's pixels, made again when it changed; a picture that cannot be read leaves the plain colour under it, and its reason is kept for the tooltip
const uint8_t *backdrop_pixels()
{
    if (g.back_dirty) {
        g.back_dirty = false, g.dirty = true;
        g.back_why[0] = '\0';
        if (!card_backdrop_fill(&g.backdrop, CARD_PHOTO_W, CARD_PHOTO_H, g.back_rgba, g.back_why, sizeof g.back_why)) {
            CardBackdrop plain = g.backdrop;
            plain.kind = CARD_BACKDROP_PLAIN;
            char ignore[8];
            card_backdrop_fill(&plain, CARD_PHOTO_W, CARD_PHOTO_H, g.back_rgba, ignore, sizeof ignore);
        }
    }
    return g.back_rgba;
}

// the race frame the Wheels choice shows: Still tire1, Fast tire4, Slow the pair tire2 / tire3 swapped at the game's rate (30 calls a second: race_sim.h)
int wheel_frame(const App *a)
{
    if (a->model_wheels == 1) return 1 + (a->model_wheels_fixed ? 0 : static_cast<int>(static_cast<long>(ImGui::GetTime() * GAME_TICKS_PER_SECOND) & 1));
    return a->model_wheels == 2 ? 3 : 0;
}

// the picture in the Preview window's light: Day or Night, the track the night is of, the light at the car, the lamps (editor/night_view.h)
void draw_picture(const App *a, int phase, int w, int h)
{
    const int wheels = wheel_frame(a);   // once: the clock moves between two calls
    g.key_wheel = wheels;
    model_scene_wheels(g.scene, wheels);
    const CarNightState night = night_settings_state(&a->night, phase * POLICE_FLASH_TICKS);
    const bool made = g.card_view ? card_photo_render(g.scene, &g.orbit, &night, backdrop_pixels(), g.rgba) : model_scene_render(g.scene, &g.orbit, w, h, &night, g.rgba);
    if (made && g.card_view && g.as_card && g.cpal_ok) {   // what Central makes of it: the same dither into the same palette
        uint8_t *drawn = static_cast<uint8_t *>(malloc(static_cast<size_t>(w) * h * 4));
        if (drawn != nullptr) {
            card_central_as_drawn(g.rgba, w, h, g.cpal, drawn);
            memcpy(g.rgba, drawn, static_cast<size_t>(w) * h * 4);
            free(drawn);
        }
    }
    if (made) SDL_UpdateTexture(g.tex, nullptr, g.rgba, w * 4);
    g.key_night = a->night, g.key_phase = phase;
    g.dirty = false;
}

// the picture's size: the whole area, scaled down to what the rasterizer takes (at most MAX_W x its rows) with the area's shape kept; 0 when too small
bool picture_size(int area_w, int area_h, int *w, int *h, int least = 64)
{
    if (area_w < least || area_h < least) return false;
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
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) orbit_home(&g.orbit), g.fit = true, g.dirty = true;
}

// «Use as card photo»: the picture as it is on screen (the Model's camera, the Preview's paint and light) at the card's 309 x 166 over a plain backdrop, in the
// card.png of the car's piece; asks before it replaces one
int use_as_card_photo(App *a)
{
    char why[APP_MSG];
    const uint8_t *backdrop = backdrop_pixels();
    if (g.back_why[0] != '\0') {   // the picture asked for cannot be read: no plain card in its place
        snprintf(a->status, sizeof a->status, "card photo: the backdrop's picture: %s", g.back_why);
        return 0;
    }
    const CarNightState night = night_settings_state(&a->night, 0);
    char dir[APP_PATH];
    car_project_card_dir(&a->car, dir, sizeof dir);   // the kit's own card.png (the build puts it in the piece), not build/piece's
    const int ok = card_photo_write(g.scene, &g.orbit, &night, backdrop, dir, why, sizeof why);
    if (ok) snprintf(a->status, sizeof a->status, "card.png made from the Model's view (309 x 166)");
    else snprintf(a->status, sizeof a->status, "card photo: %s", why);
    return ok;
}

// Control Central's palette from the game's files, read again when the files' place changes
void central_palette_of(const App *a)
{
    static bool read = false;   // (a place of "" is a place too: its reason is said)
    if (read && strcmp(g.cpal_cd, a->export_src.cd) == 0) return;
    read = true, g.dirty = true;   // the picture is made again with the palette
    snprintf(g.cpal_cd, sizeof g.cpal_cd, "%s", a->export_src.cd);
    g.cpal_why[0] = '\0';
    g.cpal_ok = a->export_src.cd[0] != '\0' && card_central_palette(a->export_src.cd, g.cpal, g.cpal_why, sizeof g.cpal_why);
    if (!g.cpal_ok && g.cpal_why[0] == '\0') snprintf(g.cpal_why, sizeof g.cpal_why, "The game's files were not found: start the editor with --cd DIR or Locate game.");
}

// the View: the car on its own, over the card's backdrop at the card's proportions (what Use as card photo saves), or in Control Central's palette as the game draws the card
void view_controls(const App *a)
{
    central_palette_of(a);
    ModelView view = model_view_of(g.card_view, g.as_card);
    for (int v = 0; v < MODEL_VIEWS; ++v) {
        if (v > 0) ImGui::SameLine();
        const bool unusable = v == MODEL_VIEW_GAME && !g.cpal_ok;
        ImGui::BeginDisabled(unusable);
        if (ImGui::RadioButton(model_view_name(static_cast<ModelView>(v)), view == v)) {
            int card = 0, as_card = 0;
            model_view_flags(static_cast<ModelView>(v), &card, &as_card);
            g.card_view = card != 0, g.as_card = as_card != 0, g.dirty = true;
        }
        ImGui::EndDisabled();
        if (unusable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", g.cpal_why);
    }
    ImGui::SameLine();
    help_marker("Model: the car on its own.\n"
                "Card: the picture at the card's 309 x 166 proportions over the card's backdrop, what Use as card photo saves. The shaded corner is where Control Central writes the car's name.\n"
                "As the game draws it: the card picture in Control Central's palette, dithered as the game does it.");
}

void backdrop_controls(App *a)
{
    static const char *const KINDS[] = { "Plain colour", "Gradient", "Picture" };
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Backdrop");   // its colour swatch is a square: named, it is not read as a checkbox of the Wheels beside it
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    if (ImGui::BeginCombo("##backdrop", KINDS[g.backdrop.kind])) {
        for (int i = 0; i < 3; ++i)
            if (ImGui::Selectable(KINDS[i], g.backdrop.kind == i)) g.backdrop.kind = i, g.back_dirty = true;
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("What the card photo stands in front of");
    float top[3] = { g.backdrop.top[0] / 255.0f, g.backdrop.top[1] / 255.0f, g.backdrop.top[2] / 255.0f };
    float bottom[3] = { g.backdrop.bottom[0] / 255.0f, g.backdrop.bottom[1] / 255.0f, g.backdrop.bottom[2] / 255.0f };
    const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
    ImGui::SameLine();
    if (ImGui::ColorEdit3("##top", top, flags)) {
        for (int k = 0; k < 3; ++k) g.backdrop.top[k] = static_cast<uint8_t>(lroundf(top[k] * 255.0f));
        if (g.backdrop.kind != CARD_BACKDROP_GRADIENT) memcpy(g.backdrop.bottom, g.backdrop.top, 3);
        g.back_dirty = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(g.backdrop.kind == CARD_BACKDROP_GRADIENT ? "The top colour" : g.backdrop.kind == CARD_BACKDROP_FILE ? "What shows through a transparent picture" : "The colour");
    if (g.backdrop.kind == CARD_BACKDROP_GRADIENT) {
        ImGui::SameLine();
        if (ImGui::ColorEdit3("##bottom", bottom, flags)) {
            for (int k = 0; k < 3; ++k) g.backdrop.bottom[k] = static_cast<uint8_t>(lroundf(bottom[k] * 255.0f));
            g.back_dirty = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("The bottom colour");
    }
    if (g.backdrop.kind == CARD_BACKDROP_FILE) {
        ImGui::SameLine();
        if (ImGui::Button("Choose...")) a->backdrop_dialog = 1;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pick a PNG picture to stand behind the car");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputTextWithHint("##backfile", "path of a PNG", g.backdrop.file, sizeof g.backdrop.file, ImGuiInputTextFlags_EnterReturnsTrue)) g.back_dirty = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("A PNG picture, fitted to cover the card's 309 x 166 (Enter to load it)");
        if (g.back_why[0] != '\0') {
            ImGui::SameLine();
            help_marker(g.back_why);
        }
    }
}

void card_photo_button(App *a, bool model_only)
{
    ImGui::BeginDisabled(g.scene == nullptr || a->car.masks_only || model_only);   // (the Model view makes no card picture: the Card row says so)
    if (ImGui::Button("Use as card photo")) {
        char dir[APP_PATH];
        car_project_card_dir(&a->car, dir, sizeof dir);
        if (card_photo_exists(dir)) ImGui::OpenPopup("Replace the card photo?");
        else use_as_card_photo(a);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Make the car's card picture (card.png, 309 x 166, Control Central's) from this view: the camera, the paint and the light as shown.%s",
                          a->car.masks_only ? "\nA masks project has no card." : "");
    if (ImGui::BeginPopupModal("Replace the card photo?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("The car already has a card.png. The new picture replaces it.");
        if (ImGui::Button("Replace")) use_as_card_photo(a), ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// the first word of a toolbar row: what the row is for
void row_label(const char *text)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", text);
    ImGui::SameLine(ImGui::CalcTextSize("Camera").x + ImGui::GetStyle().ItemSpacing.x * 3.0f);
}

void toolbar(App *a)
{
    row_label("Camera");
    if (ImGui::Button("Front")) orbit_view(&g.orbit, ORBIT_FRONT), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Side")) orbit_view(&g.orbit, ORBIT_SIDE), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Rear")) orbit_view(&g.orbit, ORBIT_REAR), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Top")) orbit_view(&g.orbit, ORBIT_TOP), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Home")) orbit_home(&g.orbit), g.fit = true, g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-rotate", &g.auto_rotate)) g.dirty = true;
    a->model_auto_rotate = g.auto_rotate ? 1 : 0;
    ImGui::SameLine();
    help_marker(g.shine && g.shines ? "Drag to turn, wheel to zoom; keys F, S, R, T and Home for the views.\n"
                                      "Shine: its tones come from the car's own colours (a race takes them from the screen's)."
                                    : "Drag to turn, wheel to zoom; keys F, S, R, T and Home for the views.");

    row_label("Look");
    if (ImGui::Checkbox("Shine", &g.shine)) g.shine_asked = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The body's shine as the race draws it, where the masks say paint or shine (matte texels stay as they are).\n"
                          "Its tones are taken from the car's own colours; in a race, from the screen's.%s",
                          g.envmap[0] != '\0' ? "\nThe kit's car_envmap.png is the sky it reflects." : "");
    ImGui::SameLine();
    static const char *const WHEELS[] = { "Still", "Slow", "Fast" };
    ImGui::TextUnformatted("Wheels");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Slow").x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f);
    if (ImGui::BeginCombo("##wheels", WHEELS[a->model_wheels])) {
        for (int i = 0; i < 3; i++)
            if (ImGui::Selectable(WHEELS[i], a->model_wheels == i)) a->model_wheels = i, g.dirty = true;
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The wheels as the race draws them at a speed.\n"
                          "Still: the car at rest or crawling (tire1).\n"
                          "Slow: turning, tire2 and tire3 swapped at the game's pace (30 a second).\n"
                          "Fast: the blur (tire4), from the speed the race stops swapping.");

    ImGui::SameLine();
    if (ImGui::Button(g.big ? "Dock back" : "Enlarge")) g.big_ask = g.big ? -1 : 1;
    ui_probe("model_enlarge");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(g.big ? "Put the Model window back in the layout where it was." : "Float the Model window at nearly the size of the Studio, to see the car larger (its camera stays). Drag its edges to resize.");
    ImGui::SameLine();
    {   // the soft mask edges of the shine: a clear switch with its state, right by Enlarge (the game's shine_soft setting is where it starts; this is for this window only)
        const bool on = g.soft != 0, inactive = g.render_scale <= 1;   // at render scale 1 the edges are a straight cut whatever the switch says
        ImGui::BeginDisabled(inactive);
        ImGui::PushStyleColor(ImGuiCol_Button, on ? ImVec4(0.18f, 0.50f, 0.28f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
        if (ImGui::Button(on ? "Soft edges: ON" : "Soft edges: OFF")) g.soft = on ? 0 : 1, shine_draw_set_soft(g.soft), g.dirty = true;
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        ui_probe("model_soft");
        ImGui::SameLine();
        help_marker("Where the body's shine meets matte paint or glass, the border is mixed by how much of each is near (an ordered dither of the game's own two tables), not cut at the texel's edge.\n"
                    "Only with Render scale 2 or 3: at 1 the edge is a straight cut, as the game draws it (this switch is then grey).\n"
                    "Starts as the game's shine_soft setting (Restomod '26 on, Stock and Restored off); this switch is only for this window and never writes your options. Best seen enlarged.");
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Render scale");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("3").x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f);
    ImGui::BeginDisabled(g.card_view);   // the card's picture is 309 x 166 whatever
    char current[8];
    snprintf(current, sizeof current, "%d", g.render_scale);
    if (ImGui::BeginCombo("##render_scale", current)) {
        for (int i = 1; i <= MODEL_RENDER_SCALE_MAX; i++) {
            char label[8];
            snprintf(label, sizeof label, "%d", i);
            if (ImGui::Selectable(label, g.render_scale == i)) g.render_scale = i, g.dirty = true;
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ui_probe("model_render_scale");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("The game's render scale (its option, 1 to 3): the car is drawn on a frame 480 lines tall at 1, 960 at 2, 1440 at 3, and shown stretched to the window, as coarse as the game's picture.\n"
                          "It starts as your option; changing it here is for this window only. A small panel is drawn at its own size: Enlarge to see the scale.%s", g.card_view ? "\nNot in the Card views: their picture is 309 x 166." : "");

    row_label("View");
    view_controls(a);

    row_label("Card");   // the backdrop and the card photo are the card's: with the View on the Model they are there, greyed, and say why
    const bool model_only = !g.card_view;
    ImGui::BeginDisabled(model_only);
    backdrop_controls(a);
    ImGui::EndDisabled();
    ImGui::SameLine();
    card_photo_button(a, model_only);
    if (model_only) {
        ImGui::SameLine();
        help_marker("Pick View: Card to see the backdrop and to make the card photo.");
    }
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

void model_panel_big(bool on)
{
    if (on != g.big) g.big_ask = on ? 1 : -1;
}

void model_panel_render_scale(int scale)
{
    g.render_scale = scale < 1 ? 1 : scale > MODEL_RENDER_SCALE_MAX ? MODEL_RENDER_SCALE_MAX : scale;
    g.dirty = true;
}

void model_panel_soft(bool on)
{
    g.soft = on ? 1 : 0;
    shine_draw_set_soft(g.soft);
    g.dirty = true;
}

void model_panel_shine(bool on)
{
    g.shine = on;
    g.shine_asked = true;
}

int model_panel_backdrop(App *a, int kind, int r, int gr, int b, int bottom)
{
    if (g.scene == nullptr || a->kind != APP_KIND_CAR || kind < CARD_BACKDROP_PLAIN || kind > CARD_BACKDROP_FILE) return 0;
    if (r < 0 || r > 255 || gr < 0 || gr > 255 || b < 0 || b > 255) return 0;
    uint8_t *colour = bottom ? g.backdrop.bottom : g.backdrop.top;
    colour[0] = static_cast<uint8_t>(r), colour[1] = static_cast<uint8_t>(gr), colour[2] = static_cast<uint8_t>(b);
    if (!bottom) {
        g.backdrop.kind = kind;
        if (kind != CARD_BACKDROP_GRADIENT) memcpy(g.backdrop.bottom, g.backdrop.top, 3);
    }
    g.back_dirty = true;
    return 1;
}

const char *model_panel_backdrop_path()
{
    return g.backdrop.file;
}

void model_panel_backdrop_file(const char *path)
{
    g.backdrop.kind = CARD_BACKDROP_FILE;
    snprintf(g.backdrop.file, sizeof g.backdrop.file, "%s", path);
    g.back_dirty = true;
}

int model_panel_card_view(App *a, bool card, bool as_card)
{
    if (g.scene == nullptr || a->kind != APP_KIND_CAR) return 0;
    g.card_view = card, g.as_card = as_card, g.dirty = true;
    return 1;
}

int model_panel_card_photo(App *a)
{
    if (g.scene == nullptr || a->kind != APP_KIND_CAR || a->car.masks_only) {
        snprintf(a->status, sizeof a->status, "card photo: there is no car in the Model window to take it from");
        return 0;
    }
    return use_as_card_photo(a);
}

void model_panel_preset(double yaw, double pitch, int dist)
{
    g.preset = 1, g.preset_yaw = yaw, g.preset_pitch = pitch, g.preset_dist = dist;
}

void model_panel_nudge(void)
{
    orbit_drag(&g.orbit, 40, 25);
    orbit_zoom(&g.orbit, 2);
    g.fit = false, g.dirty = true;
}

void model_panel_state(int *big, int *docked, int *scale, int *soft)
{
    *big = g.big, *docked = g.docked, *scale = g.render_scale, *soft = g.soft;
}

void model_panel_camera(int *yaw, int *pitch, int *dist)
{
    *yaw = g.orbit.yaw, *pitch = g.orbit.pitch, *dist = g.orbit.dist;
}

// where the docked window is, to put it back there: its node, and the node round it with the side and share it had
static void remember_dock()
{
    g.dock_id = ImGui::GetWindowDockID();
    const ImGuiDockNode *node = ImGui::DockBuilderGetNode(g.dock_id);
    const ImGuiDockNode *parent = node != nullptr ? node->ParentNode : nullptr;
    if (parent == nullptr || parent->ChildNodes[0] == nullptr || parent->ChildNodes[1] == nullptr) return;
    const bool first = parent->ChildNodes[0] == node, across = parent->SplitAxis == ImGuiAxis_X;
    const float whole = across ? parent->Size.x : parent->Size.y, mine = across ? node->Size.x : node->Size.y;
    if (whole <= 0.0f) return;
    g.parent_id = parent->ID;
    g.split_dir = across ? (first ? ImGuiDir_Left : ImGuiDir_Right) : (first ? ImGuiDir_Up : ImGuiDir_Down);
    g.split_ratio = fminf(0.9f, fmaxf(0.1f, mine / whole));
}

void model_panel(App *a, SDL_Renderer *ren)
{
    if (g.soft < 0) g.soft = car_race_player_shine_soft();   // the options the player set: this window starts as the game is
    if (g.render_scale < 1) g.render_scale = car_race_player_render_scale();
    shine_draw_set_soft(g.soft);   // this window's own choice stands over the start-up value
    shine_draw_set_render_scale(g.render_scale);   // and soft edges only from render scale 2, as in the game
    if (g.big_ask > 0) {   // Enlarge: out of the dock, over nearly the whole window, for this frame (then it is an ordinary window: resize it)
        const ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.04f, vp->Pos.y + vp->Size.y * 0.06f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(vp->Size.x * 0.92f, vp->Size.y * 0.88f), ImGuiCond_Always);
        g.big = true, g.big_ask = 0, g.dirty = true;   // the camera is not touched: the field of view is across the picture's smaller side, so the car fills the same share at any size
    } else if (g.big_ask < 0) {   // Dock back: where it was. Its own node went when it left (a window alone in its node), so the node it shared the layout with is split again as it was
        if (g.dock_id != 0 && ImGui::DockBuilderGetNode(g.dock_id) != nullptr) {
            ImGui::SetNextWindowDockID(g.dock_id, ImGuiCond_Always);
        } else if (g.parent_id != 0 && ImGui::DockBuilderGetNode(g.parent_id) != nullptr) {
            ImGuiID back = 0, rest = 0;
            ImGui::DockBuilderSplitNode(g.parent_id, g.split_dir, g.split_ratio, &back, &rest);
            ImGui::SetNextWindowDockID(back, ImGuiCond_Always);
        }
        g.big = false, g.big_ask = 0, g.dirty = true;
    }
    if (ImGui::Begin(PANEL_MODEL)) {
        g.docked = ImGui::IsWindowDocked();
        if (g.docked) remember_dock();
        const int paint = a->preview_paint;
        const ExportCar *oc = car_race_original_of(&a->car);   // the paints as the Preview window has them: a masks project and the game's traffic have none
        const int count = a->car.masks_only || (oc != nullptr && oc->kind != EXPORT_DRIVABLE) ? 0 : a->edit.now.paint_count;
        const uint8_t *rgb = paint >= 0 && paint < count ? a->edit.now.paints[paint].rgb : nullptr;
        const bool busy = a->car.cfm[0] == '\0';
        if (!busy && (strcmp(g.loaded, a->car.cfm) != 0 || g.loaded_serial != a->cdisk.serial)) load(a, rgb);
        else if (!busy && g.scene != nullptr && stale(a, rgb)) {   // a mask changed on disk, another paint: the same camera on the car made again
            open_scene(a, rgb);
            remember(a, rgb);
        }
        if (g.scene == nullptr) {
            ImGui::TextWrapped("%s", g.why[0] != 0 ? g.why : "No car is open.");
        } else {
            toolbar(a);
            keys();
            if (g.shine_asked && !busy) set_shine(a, rgb), g.dirty = true;   // the draft is of the masks as they are
            const ImVec2 size = ImGui::GetContentRegionAvail();
            const int lines = g.shine && !g.shines ? 2 : 1;   // the words under the picture: the state, and "no shine" when Shine has nothing to show
            const int area_w = static_cast<int>(size.x), area_h = static_cast<int>(size.y) - lines * static_cast<int>(ImGui::GetTextLineHeightWithSpacing());
            int w = 0, h = 0;   // the picture fills the area (car_scene keeps its pixels square, the car whole in the smaller side)
            int rw = area_w, rh = area_h;   // the frame the car is drawn on: the game's at this render scale, in the area's shape
            if (!g.card_view) model_view_render_size(area_w, area_h, g.render_scale, &rw, &rh);
            const bool sized = g.card_view ? (w = CARD_PHOTO_W, h = CARD_PHOTO_H, true) : picture_size(rw, rh, &w, &h, area_w < 64 || area_h < 64 ? 64 : 1);
            if (sized && make_target(ren, w, h)) {
                if (g.fit && !g.card_view) {   // the car as large as the picture lets it be, whatever the window's shape
                    model_scene_frame(g.scene, &g.orbit, w, h, FRAME_PERCENT);
                    g.fit = false, g.dirty = true;
                }
                if (g.auto_rotate) {
                    const int step = static_cast<int>(roundf(ImGui::GetIO().DeltaTime * AUTO_DEG_PER_SECOND * 16.0f));
                    orbit_drag(&g.orbit, -step / ORBIT_DRAG - (step % ORBIT_DRAG != 0), 0);   // at least a step a frame: the yaw moves by 1/16 degrees
                    g.dirty = true;
                }
                const int phase = a->night.police ? static_cast<int>((SDL_GetTicks() / 500u) & 1u) : 0;   // the flash, as the Preview window has it
                if (g.dirty || (g.back_dirty && g.card_view) || g.key_phase != phase || g.key_wheel != wheel_frame(a) || memcmp(&g.key_night, &a->night, sizeof a->night) != 0) draw_picture(a, phase, w, h);
                const ImVec2 at = ImGui::GetCursorScreenPos(), area(static_cast<float>(area_w), static_cast<float>(area_h));
                ImGui::InvisibleButton("##orbit", area);
                turn_and_zoom();
                ImVec2 from = at, to(at.x + area.x, at.y + area.y);
                if (g.card_view) {   // the card's proportions, as large as the area lets them be, in the middle (nearest texels: the picture is 309 x 166)
                    const float k = fminf(area.x / CARD_PHOTO_W, area.y / CARD_PHOTO_H);
                    const float fw = CARD_PHOTO_W * k, fh = CARD_PHOTO_H * k;
                    from = ImVec2(at.x + (area.x - fw) / 2, at.y + (area.y - fh) / 2), to = ImVec2(from.x + fw, from.y + fh);
                }
                ImDrawList *draw = ImGui::GetWindowDrawList();
                draw->AddImage(reinterpret_cast<ImTextureID>(g.tex), from, to);
                if (g.card_view) {   // where Control Central writes the name (measured on the game's own card, bottom left)
                    const float k = (to.x - from.x) / CARD_PHOTO_W;
                    draw->AddRectFilled(ImVec2(from.x, from.y + 142 * k), ImVec2(from.x + 112 * k, to.y), IM_COL32(0, 0, 0, 70));
                    draw->AddRect(from, to, IM_COL32(255, 255, 255, 60));
                }
                // the Preview's words: a paint chosen is the race's; none (or a car the paint cannot be made for) is the base texture
                char caption[96];
                const bool painted = g.painted && rgb != nullptr;
                model_paint_caption(painted ? a->edit.now.paints[paint].name : nullptr, painted, caption, sizeof caption);
                ImGui::TextDisabled("%s", caption);
                if (g.shine && !g.shines) {
                    ImGui::TextDisabled("Shine: nothing shines");
                    ImGui::SameLine();
                    help_marker("No texel of this car shines: its masks mark none as paint or shine.");
                }
            }
        }
    }
    ImGui::End();
}
