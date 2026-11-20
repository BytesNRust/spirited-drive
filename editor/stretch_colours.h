// Colour by's colours (dep95): the colour of a class in a mode (formats/track_trace_stretch.h). The Viewport's road line and the height profile both
// read stretch_colour, so a stretch is the same colour in both; a mode has a row of colours here and nowhere else.
#ifndef TNFS_TRACK_EDITOR_STRETCH_COLOURS_H
#define TNFS_TRACK_EDITOR_STRETCH_COLOURS_H

#include "imgui.h"

extern "C" {
#include "formats/track_trace_stretch.h"
}

namespace stretch_colours {

const int MAX_CLASSES = 8;

#define SC(r, g, b) IM_COL32(r, g, b, 255)
// each mode's classes in order (track_trace_stretch.h); Limits is the road's own grey
const ImU32 TABLE[TRACE_COLOUR_MODES][MAX_CLASSES] = {
    { SC(200, 204, 212) },                                                                                   // Limits
    { SC(200, 204, 212), SC(80, 170, 255), SC(255, 150, 60) },                                               // Curve: straight, left, right
    { SC(200, 204, 212), SC(250, 220, 90), SC(250, 150, 60), SC(235, 80, 60), SC(120, 220, 230), SC(80, 160, 240), SC(110, 90, 220) },   // Bank
    { SC(90, 120, 140), SC(80, 150, 170), SC(80, 180, 170), SC(110, 205, 160), SC(170, 225, 130), SC(235, 240, 120) },                    // Width
    { SC(200, 204, 212), SC(255, 150, 60), SC(170, 110, 240) },                                              // Lanes
    { SC(200, 204, 212), SC(80, 210, 230), SC(255, 150, 60), SC(150, 230, 90) },                             // Shoulder
    { SC(200, 204, 212), SC(170, 120, 70), SC(225, 165, 95), SC(110, 170, 230), SC(155, 210, 255), SC(190, 190, 150), SC(245, 245, 190) },   // Fence
};
#undef SC

}   // namespace stretch_colours

// the colour of `klass` in `mode`; the road's grey for anything out of range
inline ImU32 stretch_colour(int mode, int klass)
{
    if (mode < 0 || mode >= TRACE_COLOUR_MODES || klass < 0 || klass >= track_trace_class_count((TraceColourBy)mode) || klass >= stretch_colours::MAX_CLASSES)
        return stretch_colours::TABLE[TRACE_COLOUR_LIMITS][0];
    return stretch_colours::TABLE[mode][klass];
}

#endif
