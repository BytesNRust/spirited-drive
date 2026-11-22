// The masks of a car (dep59 car editor E3, read-only since dep100): a row for each texture with the state of its mask, and the picked one overlaid on its texture. Masks are painted in an external
// editor (PSD or PNG); the view reloads when the file changes (app_card_watch).
#include "help_marker.h"
#include "panels.h"

#include <SDL3/SDL_keyboard.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {

void muted(const char *format, ...) IM_FMTARGS(1);
void muted(const char *format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// the masks of the car, a row for each texture of the car file with its mask's state; a click shows that mask over its texture
void mask_list(App *a)
{
    MaskUi &m = a->mask;
    if (m.tex_count == 0) {
        muted("The car file has no textures to show (%s).", a->car.cfm[0] != '\0' ? "it could not be read or has none" : "no car file");
        return;
    }
    const char *dash_state = app_mask_dash_state(a);   // the dash's mask is made from dash.psd: a row of its own, read only (no texture to show it over)
    const float rows = fminf((float)(m.tex_count + (dash_state != nullptr ? 1 : 0)), 7.0f);
    if (ImGui::BeginTable("masklist", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * (rows + 1.2f)))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Mask", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < m.tex_count; ++i) {
            const MaskTexInfo &t = m.tex[i];
            char name[48];
            snprintf(name, sizeof name, "%s%s", t.name, t.level == 0 ? "" : " (low)");
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::BeginDisabled(!m.has_mask[i]);   // a texture with no mask: its whole row greyed
            ImGui::TableNextColumn();
            if (ImGui::Selectable(name, i == m.sel, ImGuiSelectableFlags_SpanAllColumns) && i != m.sel) app_mask_open(a, i);
            ImGui::TableNextColumn();
            ImGui::Text("%dx%d", t.width, t.height);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(app_mask_state(a, i));
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if (dash_state != nullptr) {
            ImGui::PushID("dash_row");
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("dash (dash.psd)");
            ImGui::TableNextColumn();
            ImGui::TextDisabled("-");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(dash_state);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void legend(App *a)
{
    MaskUi &m = a->mask;
    ImGui::Checkbox("Overlay", reinterpret_cast<bool *>(&m.show));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("Opacity", &m.opacity, 0.0f, 1.0f, "%.2f");
    for (int i = 0; i < ME_CLASSES; ++i) {
        const MaskClass *c = mask_edit_class(i);
        ImGui::PushID(i);
        ImGui::Checkbox("##on", reinterpret_cast<bool *>(&m.class_on[i]));
        ImGui::SameLine();
        const ImVec4 col(c->rgb[0] / 255.0f, c->rgb[1] / 255.0f, c->rgb[2] / 255.0f, 1.0f);
        ImGui::ColorButton("##swatch", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(16, 16));
        ImGui::SameLine();
        ImGui::Text("%s  (%d)", c->name, c->value);
        ImGui::PopID();
    }
}

// The zoom a key of the keyboard asks for, as the keyboard's layout names the key: ImGui names the punctuation keys by their place on a US keyboard (a Spanish
// keyboard's '+' arrives as RightBracket), so the place is turned back into the layout's character. ME_ZOOM_NONE for none.
MaskZoomKey zoom_key_pressed()
{
    static const struct { ImGuiKey key; SDL_Scancode place; } PLACES[] = {
        { ImGuiKey_Minus, SDL_SCANCODE_MINUS },         { ImGuiKey_Equal, SDL_SCANCODE_EQUALS },         { ImGuiKey_LeftBracket, SDL_SCANCODE_LEFTBRACKET },
        { ImGuiKey_RightBracket, SDL_SCANCODE_RIGHTBRACKET }, { ImGuiKey_Backslash, SDL_SCANCODE_BACKSLASH }, { ImGuiKey_Semicolon, SDL_SCANCODE_SEMICOLON },
        { ImGuiKey_Apostrophe, SDL_SCANCODE_APOSTROPHE }, { ImGuiKey_GraveAccent, SDL_SCANCODE_GRAVE },  { ImGuiKey_Comma, SDL_SCANCODE_COMMA },
        { ImGuiKey_Period, SDL_SCANCODE_PERIOD },        { ImGuiKey_Slash, SDL_SCANCODE_SLASH },          { ImGuiKey_Oem102, SDL_SCANCODE_NONUSBACKSLASH },
    };
    const ImGuiIO &io = ImGui::GetIO();
    if (io.KeyCtrl || io.KeySuper || io.KeyAlt || io.WantTextInput) return ME_ZOOM_NONE;   // a shortcut, or a field being typed in
    if (ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) return ME_ZOOM_IN;
    if (ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) return ME_ZOOM_OUT;
    if (ImGui::IsKeyPressed(ImGuiKey_0, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)) return ME_ZOOM_FIT;
    for (const auto &k : PLACES) {
        if (!ImGui::IsKeyPressed(k.key)) continue;
        const MaskZoomKey z = mask_edit_zoom_key((int)SDL_GetKeyFromScancode(k.place, SDL_KMOD_NONE, false));
        if (z != ME_ZOOM_NONE) return z;
    }
    return ME_ZOOM_NONE;
}

// the view of the canvas the frame before: its scroll and the size of its content area, whether the pointer was on the picture (the wheel zooms there), the wheel's part
// of a notch, a pan by the right or middle button under way; for the mask `sel` drawn in frame `frame` (another mask, or a frame without the canvas, starts afresh)
struct CanvasView {
    ImVec2 scroll, size;
    bool hovered, panning;
    float wheel;
    int sel, frame;
};
CanvasView g_view = { ImVec2(0, 0), ImVec2(0, 0), false, false, 0.0f, -1, -1 };

// the view as it is now, once the picture and its button are drawn; the right or middle button dragged on the picture moves the view (the left one paints)
void canvas_view_update(const MaskUi &m, bool hovered)
{
    CanvasView &v = g_view;
    const ImGuiStyle &st = ImGui::GetStyle();
    const ImVec2 win = ImGui::GetWindowSize();   // less the scroll bars and the padding: the area the picture shows in, whose middle the zoom keeps
    v.size = ImVec2(win.x - (ImGui::GetScrollMaxY() > 0 ? st.ScrollbarSize : 0.0f) - 2.0f * st.WindowPadding.x,
                    win.y - (ImGui::GetScrollMaxX() > 0 ? st.ScrollbarSize : 0.0f) - 2.0f * st.WindowPadding.y);
    if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) v.panning = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) v.panning = false;
    if (v.panning) {
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        ImGui::SetScrollX(ImGui::GetScrollX() - d.x);
        ImGui::SetScrollY(ImGui::GetScrollY() - d.y);
    }
    v.scroll = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
    v.hovered = hovered;
    v.sel = m.sel;
    v.frame = ImGui::GetFrameCount();
}

// the picture, scaled, read-only. The slider, the keys + - 0 (the window has the keyboard) and the wheel on the
// picture zoom about the middle of the view.
void canvas(App *a, ImTextureID tex, float height)
{
    MaskUi &m = a->mask;
    MaskEdit &e = m.me;
    CanvasView &view = g_view;
    if (tex == 0) return;
    if (view.sel != m.sel || view.frame != ImGui::GetFrameCount() - 1) view = CanvasView{ ImVec2(0, 0), ImVec2(0, 0), false, false, 0.0f, m.sel, -1 };
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    float scale = m.zoom;
    if (scale <= 0) {   // fit the window, whole texels
        scale = floorf(fminf((avail.x - 8) / (float)e.width, (avail.y - 40) / (float)e.height) * 2.0f) / 2.0f;
        if (scale < (float)ME_ZOOM_MIN) scale = (float)ME_ZOOM_MIN;
        if (scale > (float)ME_ZOOM_MAX) scale = (float)ME_ZOOM_MAX;
    }
    const float before = scale;
    ImGui::SetNextItemWidth(160);
    float zoom = scale;
    if (ImGui::SliderFloat("Zoom", &zoom, (float)ME_ZOOM_MIN, (float)ME_ZOOM_MAX, "%.1fx")) scale = m.zoom = zoom;
    ImGui::SameLine();
    bool fit = ImGui::Button("Fit");
    ImGui::SameLine();
    help_marker("Zoom: + - 0 or the wheel. Move: right or middle drag.\nMasks are painted in an external editor (the PSD or the PNG); this window shows the result and reloads when the file changes.");
    const MaskZoomKey key = m.focus ? zoom_key_pressed() : ME_ZOOM_NONE;
    const int notches = view.hovered ? mask_edit_wheel_steps(&view.wheel, ImGui::GetIO().MouseWheel) : 0;
    if (key == ME_ZOOM_IN) scale = m.zoom = mask_edit_zoom_step(scale, 1);
    else if (key == ME_ZOOM_OUT) scale = m.zoom = mask_edit_zoom_step(scale, -1);
    for (int n = notches; n != 0; n += n > 0 ? -1 : 1) scale = m.zoom = mask_edit_zoom_step(scale, n > 0 ? 1 : -1);
    fit |= key == ME_ZOOM_FIT;
    if (fit) m.zoom = 0;   // the next frame fits, from the start
    const ImVec2 size(e.width * scale, e.height * scale);
    if (fit) {
        ImGui::SetNextWindowScroll(ImVec2(0, 0));
    } else if (scale != before) {   // the content's size is given so that the new scroll is not held to the old picture's
        ImGui::SetNextWindowContentSize(size);
        ImGui::SetNextWindowScroll(ImVec2(mask_edit_zoom_scroll(view.scroll.x, view.size.x, before, scale), mask_edit_zoom_scroll(view.scroll.y, view.size.y, before, scale)));
    }
    ImGui::BeginChild("canvas", ImVec2(0, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *pic = ImGui::GetWindowDrawList();
    pic->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);   // hard texels at any zoom, not blurred
    ImGui::Image(tex, size);
    pic->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("view", size);
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);   // the wheel on the picture zooms, it does not scroll
    canvas_view_update(m, ImGui::IsItemHovered());
    ImGui::EndChild();
}

}   // namespace

// The Masks window's list of masks and, under it, the open mask over its texture. `tex` is the mask over its texture (made by the caller from mask_edit_compose),
// 0 when no mask is open. Inside the Masks window (masks_panel), which says when it has the keyboard (MaskUi.focus).
void mask_view_section(App *a, ImTextureID tex)
{
    MaskUi &m = a->mask;
    mask_list(a);
    if (m.tex_count == 0) return;
    if (m.error[0] != '\0') ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m.error);
    if (m.sel < 0) {
        muted("Choose a texture to see its mask over it.");
        return;
    }
    static int brought = -1;   // a mask just picked (or opened by name) brings the view into sight: the list above may fill the window
    if (m.sel != brought) {
        brought = m.sel;
        ImGui::SetScrollHereY(0.0f);
    }
    ImGui::Text("%s   %dx%d", m.tex[m.sel].file, m.me.width, m.me.height);
    if (strcmp(app_mask_state(a, m.sel), "edited by hand") == 0) {   // not what the PSD makes: Convert PSD keeps it, Regenerate from PSD replaces it
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.69f, 0.13f, 1.0f), "edited by hand");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("This PNG is not what its PSD makes: it was edited after Studio made it.\nConvert PSD keeps it; Regenerate from PSD (in Masks) replaces it.");
    }
    if (m.tex_file[0] != '\0') {   // only in a kit: the texture's PNG in its bank folder
        ImGui::SameLine();
        if (ImGui::Button("Show texture file")) snprintf(a->reveal, sizeof a->reveal, "%s", m.tex_file);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show this texture's PNG in the Finder, to paint it in an external image editor.\n%s", m.tex_file);
    }
    if (m.note[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
        ImGui::TextWrapped("%s", m.note);
        ImGui::PopStyleColor();
    }
    const float height = fmaxf(ImGui::GetContentRegionAvail().y, 260.0f);   // the window scrolls when the view has too little room
    ImGui::BeginChild("tools", ImVec2(280, height), ImGuiChildFlags_None);
    legend(a);
    ImGui::Spacing();
    ImGui::BeginDisabled(!(car_race_original_of(&a->car) != nullptr && !app_race_busy(a)));
    if (ImGui::Button("In-game photo")) app_race(a, 1);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Runs the game on the masks on disk and shows the picture in the Race window");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    canvas(a, tex, height);
    ImGui::EndGroup();
}
