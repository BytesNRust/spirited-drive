// The Edit mask window (dep59 car editor E3): pick a texture of the car, see it with its mask overlaid, paint the mask (pencil, fill, eyedropper), undo, Save.
#include "panels.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *const PANEL_MASK_EDIT = "Edit mask";

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

void texture_label(const MaskTexInfo &t, bool has_mask, char *out, size_t cap)
{
    snprintf(out, cap, "%s%s  %dx%d%s", t.name, t.level == 0 ? "" : " (low)", t.width, t.height, has_mask ? "" : "  - no mask");
}

// the texture picker; a change away from a mask with unsaved changes waits for the answer (`pending`)
void texture_picker(App *a, int &pending)
{
    MaskUi &m = a->mask;
    char label[96] = "choose a texture";
    if (m.sel >= 0) texture_label(m.tex[m.sel], true, label, sizeof label);
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("Texture", label)) {
        for (int i = 0; i < m.tex_count; ++i) {
            char row[96];
            texture_label(m.tex[i], m.has_mask[i] != 0, row, sizeof row);
            ImGui::BeginDisabled(!m.has_mask[i]);
            if (ImGui::Selectable(row, i == m.sel) && i != m.sel) {
                if (app_mask_modified(a)) pending = i;
                else app_mask_open(a, i);
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
}

void toolbar(App *a)
{
    MaskUi &m = a->mask;
    const char *const names[] = { "Pencil", "Fill", "Eyedropper" };
    for (int t = 0; t < 3; ++t) {
        if (t > 0) ImGui::SameLine();
        if (ImGui::RadioButton(names[t], m.tool == t)) m.tool = t;
    }
    if (m.tool == MASK_TOOL_PENCIL) {
        ImGui::SetNextItemWidth(160);
        ImGui::SliderInt("Size (px)", &m.brush, ME_BRUSH_MIN, ME_BRUSH_MAX);
    } else if (m.tool == MASK_TOOL_FILL) {
        if (ImGui::RadioButton("Same mask class", m.fill_by_texture == 0)) m.fill_by_texture = 0;
        ImGui::SameLine();
        ImGui::BeginDisabled(m.me.tex == nullptr);
        if (ImGui::RadioButton("Same texture colour", m.fill_by_texture == 1)) m.fill_by_texture = 1;
        ImGui::EndDisabled();
    } else {
        muted("Click a texel to take its class as the one to paint with.");
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
        char row[96];
        snprintf(row, sizeof row, "%s  (%d)", c->name, c->value);
        if (ImGui::Selectable(row, m.cls == i)) m.cls = i;   // the class the pencil and the fill paint with
        ImGui::PopID();
    }
}

// the picture, scaled; the pencil, fill and eyedropper act on the texel under the mouse
void canvas(App *a, ImTextureID tex)
{
    MaskUi &m = a->mask;
    MaskEdit &e = m.me;
    if (tex == 0) return;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    float scale = m.zoom;
    if (scale <= 0) {   // fit the window, whole texels
        scale = floorf(fminf((avail.x - 8) / (float)e.width, (avail.y - 40) / (float)e.height) * 2.0f) / 2.0f;
        if (scale < 1.0f) scale = 1.0f;
        if (scale > 16.0f) scale = 16.0f;
    }
    ImGui::SetNextItemWidth(160);
    float zoom = scale;
    if (ImGui::SliderFloat("Zoom", &zoom, 1.0f, 16.0f, "%.1fx")) m.zoom = zoom;
    ImGui::SameLine();
    if (ImGui::Button("Fit")) m.zoom = 0;
    ImGui::SameLine();
    muted("Pencil: drag. Ctrl+Z undoes.");
    ImGui::BeginChild("canvas", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos(), size(e.width * scale, e.height * scale);
    ImGui::Image(tex, size);
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("paint", size);
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const int x = (int)floorf((mouse.x - origin.x) / scale), y = (int)floorf((mouse.y - origin.y) / scale);
    const bool inside = x >= 0 && y >= 0 && x < e.width && y < e.height;
    static int last_x = -1, last_y = -1;
    const uint8_t cls = mask_edit_class(m.cls)->value;
    if (hovered && inside) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (m.tool == MASK_TOOL_PENCIL) {
                mask_edit_stroke_begin(&e);
                mask_edit_pencil(&e, x, y, m.brush, cls);
                last_x = x, last_y = y;
            } else if (m.tool == MASK_TOOL_FILL) {
                mask_edit_fill(&e, x, y, cls, m.fill_by_texture ? ME_FILL_TEXTURE : ME_FILL_CLASS);
            } else {
                m.cls = mask_edit_class_at(&e, x, y);
            }
        }
        ImDrawList *dl = ImGui::GetWindowDrawList();   // the texel (or the pencil's square) the click would change
        const int size_px = m.tool == MASK_TOOL_PENCIL ? m.brush : 1, x0 = x - (size_px - 1) / 2, y0 = y - (size_px - 1) / 2;
        dl->AddRect(ImVec2(origin.x + x0 * scale, origin.y + y0 * scale), ImVec2(origin.x + (x0 + size_px) * scale, origin.y + (y0 + size_px) * scale), IM_COL32(255, 255, 255, 255));
    }
    if (m.tool == MASK_TOOL_PENCIL && e.stroke != 0) {
        if (active && inside && (x != last_x || y != last_y)) {
            mask_edit_line(&e, last_x, last_y, x, y, m.brush, cls);
            last_x = x, last_y = y;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) mask_edit_stroke_end(&e);
    }
    ImGui::EndChild();
}

}   // namespace

// Edit mask: the window itself. `tex` is the mask over its texture (made by the caller from mask_edit_compose), 0 when no mask is open.
void mask_edit_panel(App *a, ImTextureID tex)
{
    static int pending = -1;   // a texture chosen while the open mask has unsaved changes: Save, Discard or Cancel decides
    MaskUi &m = a->mask;
    static int shown = -1, wait = 0;   // a mask just opened brings its window forward (after the dock layout has settled: a focus given in that frame is lost)
    if (m.sel != shown) {
        if (m.sel >= 0) wait = 3;
        shown = m.sel;
    }
    if (wait > 0 && --wait == 0) ImGui::SetNextWindowFocus();
    const bool open = ImGui::Begin(PANEL_MASK_EDIT);
    m.focus = open && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (open) {
        const bool busy = app_rehacer_busy(a);
        if (m.tex_count == 0) {
            muted("The car file has no textures to edit (%s).", a->car.cfm[0] != '\0' ? "it could not be read or has none" : "no car file");
        } else {
            ImGui::BeginDisabled(busy);
            texture_picker(a, pending);
            if (m.sel >= 0) {
                ImGui::SameLine();
                if (ImGui::Button("Undo")) app_undo(a);   // the keyboard's Ctrl+Z reaches here too
                ImGui::SameLine();
                if (ImGui::Button("Redo")) app_redo(a);
                ImGui::SameLine();
                ImGui::BeginDisabled(!app_mask_modified(a));
                if (ImGui::Button("Save mask")) app_mask_save(a);
                ImGui::SameLine();
                if (ImGui::Button("Discard")) {
                    const int keep = m.sel;
                    mask_edit_free(&m.me);
                    m.sel = -1;
                    app_mask_open(a, keep);
                }
                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();
            if (pending >= 0) {
                ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.0f), "This mask has unsaved changes.");
                ImGui::SameLine();
                if (ImGui::Button("Save and switch") && app_mask_save(a)) { app_mask_open(a, pending); pending = -1; }
                ImGui::SameLine();
                if (ImGui::Button("Discard and switch")) {
                    const int to = pending;
                    mask_edit_free(&m.me);
                    m.sel = -1;
                    app_mask_open(a, to);
                    pending = -1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) pending = -1;
            }
            if (m.error[0] != '\0') ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m.error);
            if (m.sel < 0) {
                muted("Choose a texture to see its mask over it and paint it. Only the textures with a mask in car_masks/%s can be edited.", a->car.folder);
            } else {
                ImGui::Text("%s   %dx%d   %s", m.tex[m.sel].file, m.me.width, m.me.height, app_mask_modified(a) ? "(unsaved changes)" : "");
                if (m.note[0] != '\0') {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
                    ImGui::TextWrapped("%s", m.note);
                    ImGui::PopStyleColor();
                }
                ImGui::BeginChild("tools", ImVec2(280, 0), ImGuiChildFlags_None);
                toolbar(a);
                ImGui::Separator();
                legend(a);
                ImGui::Spacing();
                ImGui::BeginDisabled(!(car_race_original_of(&a->car) != nullptr && !app_race_busy(a)));
                if (ImGui::Button(app_mask_modified(a) ? "Save, then Race photo" : "Race photo")) {
                    if (!app_mask_modified(a) || app_mask_save(a)) app_race(a, 1);
                }
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Runs the game on the saved masks and shows the picture in the Race window");
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginGroup();
                canvas(a, tex);
                ImGui::EndGroup();
            }
        }
    }
    ImGui::End();
}
